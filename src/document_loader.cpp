#include "document_loader.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {
const std::set<std::string> kSupported = {
    ".pdf", ".md", ".markdown", ".txt", ".py", ".js", ".ts", ".tsx", ".jsx",
    ".json", ".csv", ".yaml", ".yml", ".hpp", ".h", ".cpp", ".cc", ".c",
    ".doc", ".docx"
};

std::string lower_ext(const fs::path& path) {
    std::string ext = path.extension().string();
    for (char& ch : ext) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return ext;
}

bool is_hidden_path(const fs::path& path) {
    for (const auto& part : path) {
        std::string value = part.string();
        if (!value.empty() && value[0] == '.') {
            return true;
        }
    }
    return false;
}

std::string read_text_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot read file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string shell_quote(const std::string& value) {
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out += ch;
        }
    }
    out += "'";
    return out;
}

std::string run_command(const std::string& command) {
    std::array<char, 4096> buffer{};
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run command: " + command);
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
    }
    int status = pclose(pipe);
    if (status != 0) {
        throw std::runtime_error("Command failed: " + command);
    }
    return output;
}

std::vector<DocumentChunk> load_pdf(const fs::path& path) {
    std::vector<DocumentChunk> docs;

    std::string command = "pdftotext -layout " + shell_quote(path.string()) + " - 2>/dev/null";
    std::string text = run_command(command);
    if (text.empty()) {
        docs.push_back({path.string(), path.filename().string(), 1, ""});
        return docs;
    }

    int page = 1;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\f', start);
        std::string page_text = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!page_text.empty()) {
            docs.push_back({path.string(), path.filename().string(), page, page_text});
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
        ++page;
    }
    return docs;
}

DocumentChunk load_word_document(const fs::path& path) {
    std::string command = "textutil -convert txt -stdout " + shell_quote(path.string()) + " 2>/dev/null";
    return {path.string(), path.filename().string(), -1, run_command(command)};
}
}

std::vector<fs::path> list_supported_files(const fs::path& root) {
    std::vector<fs::path> files;
    if (fs::is_regular_file(root)) {
        if (kSupported.contains(lower_ext(root))) {
            files.push_back(root);
        }
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (!is_hidden_path(path) && kSupported.contains(lower_ext(path))) {
            files.push_back(path);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<DocumentChunk> load_documents(const fs::path& root) {
    std::vector<DocumentChunk> docs;
    for (const auto& path : list_supported_files(root)) {
        if (lower_ext(path) == ".pdf") {
            auto pages = load_pdf(path);
            docs.insert(docs.end(), pages.begin(), pages.end());
        } else if (lower_ext(path) == ".doc" || lower_ext(path) == ".docx") {
            docs.push_back(load_word_document(path));
        } else {
            docs.push_back({path.string(), path.filename().string(), -1, read_text_file(path)});
        }
    }
    return docs;
}
