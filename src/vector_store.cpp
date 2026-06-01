#include "vector_store.hpp"

#include <sqlite3.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace {
void exec_sql(sqlite3* db, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error ? error : "unknown sqlite error";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

std::string pack_vector(const std::vector<double>& values) {
    std::string bytes;
    bytes.resize(values.size() * sizeof(double));
    std::memcpy(bytes.data(), values.data(), bytes.size());
    return bytes;
}

std::vector<double> unpack_vector(const void* data, int bytes) {
    if (bytes <= 0 || bytes % static_cast<int>(sizeof(double)) != 0) {
        return {};
    }
    std::vector<double> values(static_cast<std::size_t>(bytes) / sizeof(double));
    std::memcpy(values.data(), data, static_cast<std::size_t>(bytes));
    return values;
}

double cosine_similarity(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.empty() || a.size() != b.size()) {
        return -1.0;
    }
    double dot = 0.0;
    double na = 0.0;
    double nb = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na == 0.0 || nb == 0.0) {
        return -1.0;
    }
    return dot / (std::sqrt(na) * std::sqrt(nb));
}
}

VectorStore::VectorStore(std::filesystem::path db_path) : db_path_(std::move(db_path)) {
    open();
    migrate();
}

VectorStore::~VectorStore() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void VectorStore::open() {
    std::filesystem::create_directories(db_path_.parent_path());
    if (sqlite3_open(db_path_.string().c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open sqlite database: " + db_path_.string());
    }
}

void VectorStore::migrate() {
    exec_sql(db_, "CREATE TABLE IF NOT EXISTS chunks ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                  "source TEXT NOT NULL,"
                  "file_name TEXT NOT NULL,"
                  "page INTEGER,"
                  "text TEXT NOT NULL,"
                  "embedding BLOB NOT NULL"
                  ");");
}

void VectorStore::reset() {
    exec_sql(db_, "DELETE FROM chunks;");
}

void VectorStore::add_chunk(const DocumentChunk& chunk, const std::vector<double>& embedding) {
    const char* sql = "INSERT INTO chunks (source, file_name, page, text, embedding) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare insert");
    }

    std::string packed = pack_vector(embedding);
    sqlite3_bind_text(stmt, 1, chunk.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, chunk.file_name.c_str(), -1, SQLITE_TRANSIENT);
    if (chunk.page > 0) {
        sqlite3_bind_int(stmt, 3, chunk.page);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_text(stmt, 4, chunk.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 5, packed.data(), static_cast<int>(packed.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert chunk");
    }
    sqlite3_finalize(stmt);
}

std::vector<SearchResult> VectorStore::search(const std::vector<double>& query_embedding, int k) const {
    const char* sql = "SELECT id, source, file_name, page, text, embedding FROM chunks;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare search");
    }

    std::vector<SearchResult> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        DocumentChunk chunk;
        chunk.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        chunk.file_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        chunk.page = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? -1 : sqlite3_column_int(stmt, 3);
        chunk.text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        auto embedding = unpack_vector(sqlite3_column_blob(stmt, 5), sqlite3_column_bytes(stmt, 5));
        results.push_back({id, chunk, cosine_similarity(query_embedding, embedding)});
    }
    sqlite3_finalize(stmt);

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });
    if (results.size() > static_cast<std::size_t>(k)) {
        results.resize(static_cast<std::size_t>(k));
    }

    std::set<int> ids;
    std::vector<SearchResult> expanded_results;
    for (const auto& result : results) {
        ids.insert(result.id);
        expanded_results.push_back(result);
    }

    const char* neighbor_sql =
        "SELECT id, source, file_name, page, text FROM chunks "
        "WHERE source = ? AND id BETWEEN ? AND ? ORDER BY id;";
    for (const auto& result : results) {
        sqlite3_stmt* neighbor = nullptr;
        if (sqlite3_prepare_v2(db_, neighbor_sql, -1, &neighbor, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare neighbor search");
        }
        sqlite3_bind_text(neighbor, 1, result.chunk.source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(neighbor, 2, result.id - 2);
        sqlite3_bind_int(neighbor, 3, result.id + 2);

        while (sqlite3_step(neighbor) == SQLITE_ROW) {
            int id = sqlite3_column_int(neighbor, 0);
            if (ids.contains(id)) {
                continue;
            }
            DocumentChunk chunk;
            chunk.source = reinterpret_cast<const char*>(sqlite3_column_text(neighbor, 1));
            chunk.file_name = reinterpret_cast<const char*>(sqlite3_column_text(neighbor, 2));
            chunk.page = sqlite3_column_type(neighbor, 3) == SQLITE_NULL ? -1 : sqlite3_column_int(neighbor, 3);
            chunk.text = reinterpret_cast<const char*>(sqlite3_column_text(neighbor, 4));
            expanded_results.push_back({id, chunk, result.score * 0.95});
            ids.insert(id);
        }
        sqlite3_finalize(neighbor);
    }

    return expanded_results;
}
