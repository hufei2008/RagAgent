#pragma once

#include <filesystem>
#include <string>

struct Config {
    std::string llm_model = "gemma4:26b-a4b-it-q4_K_M";
    std::string embedding_model = "bge-m3";
    std::string ollama_base_url = "http://localhost:11434";
    std::filesystem::path db_path = "storage/knowledge.sqlite3";
    std::size_t chunk_size = 800;
    std::size_t chunk_overlap = 120;
    int retrieval_k = 5;
};

Config load_config();

