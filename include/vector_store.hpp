#pragma once

#include "document.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct SearchResult {
    int id = 0;
    DocumentChunk chunk;
    double score = 0.0;
};

class VectorStore {
public:
    explicit VectorStore(std::filesystem::path db_path);
    ~VectorStore();

    VectorStore(const VectorStore&) = delete;
    VectorStore& operator=(const VectorStore&) = delete;

    void reset();
    void add_chunk(const DocumentChunk& chunk, const std::vector<double>& embedding);
    std::vector<SearchResult> search(const std::vector<double>& query_embedding, int k) const;

private:
    void open();
    void migrate();

    std::filesystem::path db_path_;
    struct sqlite3* db_ = nullptr;
};
