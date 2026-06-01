#include "config.hpp"

#include <cstdlib>
#include <fstream>
#include <map>

namespace {
std::map<std::string, std::string> read_dotenv() {
    std::ifstream file(".env");
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        values[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return values;
}

std::string env_or(const std::map<std::string, std::string>& dotenv, const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value && *value) {
        return std::string(value);
    }
    auto found = dotenv.find(name);
    return found == dotenv.end() || found->second.empty() ? fallback : found->second;
}

int env_int_or(const std::map<std::string, std::string>& dotenv, const char* name, int fallback) {
    const char* value = std::getenv(name);
    std::string raw;
    if (value && *value) {
        raw = value;
    } else if (auto found = dotenv.find(name); found != dotenv.end()) {
        raw = found->second;
    }
    if (raw.empty()) return fallback;
    try {
        return std::stoi(raw);
    } catch (...) {
        return fallback;
    }
}
}

Config load_config() {
    Config config;
    auto dotenv = read_dotenv();
    config.llm_model = env_or(dotenv, "LLM_MODEL", config.llm_model);
    config.embedding_model = env_or(dotenv, "EMBEDDING_MODEL", config.embedding_model);
    config.ollama_base_url = env_or(dotenv, "OLLAMA_BASE_URL", config.ollama_base_url);
    config.db_path = env_or(dotenv, "DB_PATH", config.db_path.string());
    config.chunk_size = static_cast<std::size_t>(env_int_or(dotenv, "CHUNK_SIZE", static_cast<int>(config.chunk_size)));
    config.chunk_overlap = static_cast<std::size_t>(env_int_or(dotenv, "CHUNK_OVERLAP", static_cast<int>(config.chunk_overlap)));
    config.retrieval_k = env_int_or(dotenv, "RETRIEVAL_K", config.retrieval_k);
    return config;
}
