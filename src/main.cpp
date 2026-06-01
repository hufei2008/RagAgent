#include "config.hpp"
#include "document_loader.hpp"
#include "ollama_client.hpp"
#include "text_splitter.hpp"
#include "vector_store.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>

namespace {
using Clock = std::chrono::steady_clock;

double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void print_usage() {
    std::cout << "Usage:\n"
              << "  local_agent index --path data/docs [--reset]\n"
              << "  local_agent chat [--question \"...\"]\n";
}

std::string arg_value(int argc, char** argv, const std::string& name, const std::string& fallback = "") {
    for (int i = 0; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

bool has_arg(int argc, char** argv, const std::string& name) {
    for (int i = 0; i < argc; ++i) {
        if (argv[i] == name) {
            return true;
        }
    }
    return false;
}

std::string source_label(const DocumentChunk& chunk) {
    if (chunk.page > 0) {
        return chunk.file_name + "，第 " + std::to_string(chunk.page) + " 页";
    }
    return chunk.file_name;
}

std::string build_prompt(const std::string& question, const std::vector<SearchResult>& results) {
    std::ostringstream prompt;
    prompt << "你是一个运行在 Mac 本地的个人知识库助手。\n"
           << "你只能根据提供的上下文回答问题。如果上下文不足，请明确说资料不足，不要编造。\n"
           << "回答使用中文，结构清晰。程序会在回答后单独展示来源，所以不要重复列出来源列表。\n\n"
           << "问题：\n" << question << "\n\n相关资料：\n";

    int index = 1;
    for (const auto& result : results) {
        prompt << "\n[片段 " << index++ << " | " << source_label(result.chunk) << "]\n"
               << result.chunk.text << "\n";
    }
    prompt << "\n请基于相关资料回答问题。";
    return prompt.str();
}

void print_search_results(const std::vector<SearchResult>& results) {
    std::cout << "\n检索调试：共 " << results.size() << " 个 chunk\n";
    int index = 1;
    for (const auto& result : results) {
        std::string preview = result.chunk.text.substr(0, 160);
        for (char& ch : preview) {
            if (ch == '\n' || ch == '\r' || ch == '\t') {
                ch = ' ';
            }
        }

        std::cout << index++ << ". "
                  << "id=" << result.id
                  << ", score=" << result.score
                  << ", source=" << source_label(result.chunk)
                  << "\n   text=" << preview;
        if (result.chunk.text.size() > preview.size()) {
            std::cout << "...";
        }
        std::cout << "\n";
    }
}

std::vector<SearchResult> select_context_results(std::vector<SearchResult> results, std::size_t max_results) {
    if (results.size() > max_results) {
        std::vector<SearchResult> selected;
        selected.reserve(max_results);

        const std::string primary_source = results.front().chunk.source;
        for (const auto& result : results) {
            if (result.chunk.source == primary_source) {
                selected.push_back(result);
                if (selected.size() == max_results) {
                    return selected;
                }
            }
        }

        for (const auto& result : results) {
            bool already_selected = false;
            for (const auto& selected_result : selected) {
                if (selected_result.id == result.id) {
                    already_selected = true;
                    break;
                }
            }
            if (!already_selected) {
                selected.push_back(result);
                if (selected.size() == max_results) {
                    return selected;
                }
            }
        }
        return selected;
    }
    return results;
}

int run_index(int argc, char** argv, const Config& config) {
    std::filesystem::path path = arg_value(argc, argv, "--path", "data/docs");
    if (!std::filesystem::exists(path)) {
        std::cerr << "Path does not exist: " << path << "\n";
        return 1;
    }

    auto docs = load_documents(path);
    auto chunks = split_documents(docs, config);
    VectorStore store(config.db_path);
    if (has_arg(argc, argv, "--reset")) {
        store.reset();
    }

    OllamaClient ollama(config);
    int indexed = 0;
    for (const auto& chunk : chunks) {
        std::cout << "Embedding " << chunk.file_name;
        if (chunk.page > 0) {
            std::cout << " page " << chunk.page;
        }
        std::cout << "...\n";
        store.add_chunk(chunk, ollama.embed(chunk.text));
        ++indexed;
    }

    std::cout << "Indexed " << docs.size() << " document pages/files into " << indexed << " chunks.\n";
    return 0;
}

void ask_question(const std::string& question, const Config& config) {
    auto t0 = Clock::now();
    OllamaClient ollama(config);
    auto t_client = Clock::now();
    VectorStore store(config.db_path);
    auto t_store = Clock::now();

    auto query_embedding = ollama.embed(question);
    auto t_embed = Clock::now();

    auto results = store.search(query_embedding, config.retrieval_k);
    results = select_context_results(std::move(results), 3);
    auto t_search = Clock::now();
    if (results.empty()) {
        std::cout << "知识库里还没有内容。请先运行 index 命令。\n";
        return;
    }
    print_search_results(results);
    auto t_debug = Clock::now();

    auto prompt = build_prompt(question, results);
    auto t_prompt = Clock::now();

    auto answer = ollama.generate(prompt);
    auto t_generate = Clock::now();

    std::cout << "\n回答：\n" << answer << "\n";
    std::cout << "\n检索来源：\n";

    std::set<std::string> seen;
    int index = 1;
    for (const auto& result : results) {
        std::string label = source_label(result.chunk);
        if (seen.insert(label).second) {
            std::cout << index++ << ". " << label << "\n";
        }
    }
    auto t_sources = Clock::now();

    std::cout << "\n耗时统计：\n"
              << "client_init_ms=" << elapsed_ms(t0, t_client) << "\n"
              << "sqlite_open_ms=" << elapsed_ms(t_client, t_store) << "\n"
              << "query_embedding_ms=" << elapsed_ms(t_store, t_embed) << "\n"
              << "sqlite_search_ms=" << elapsed_ms(t_embed, t_search) << "\n"
              << "debug_print_ms=" << elapsed_ms(t_search, t_debug) << "\n"
              << "prompt_build_ms=" << elapsed_ms(t_debug, t_prompt) << "\n"
              << "llm_generate_ms=" << elapsed_ms(t_prompt, t_generate) << "\n"
              << "source_print_ms=" << elapsed_ms(t_generate, t_sources) << "\n"
              << "total_ms=" << elapsed_ms(t0, t_sources) << "\n";
}

int run_chat(int argc, char** argv, const Config& config) {
    std::string question = arg_value(argc, argv, "--question");
    if (!question.empty()) {
        ask_question(question, config);
        return 0;
    }

    std::cout << "Local Gemma Knowledge Agent C++. 输入 exit / quit 结束。\n";
    while (true) {
        std::cout << "\n你：";
        if (!std::getline(std::cin, question)) {
            break;
        }
        if (question == "exit" || question == "quit" || question == "q") {
            break;
        }
        if (!question.empty()) {
            ask_question(question, config);
        }
    }
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    try {
        Config config = load_config();
        std::string command = argv[1];
        if (command == "index") {
            return run_index(argc, argv, config);
        }
        if (command == "chat") {
            return run_chat(argc, argv, config);
        }
        print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
