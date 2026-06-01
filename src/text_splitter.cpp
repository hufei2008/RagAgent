#include "text_splitter.hpp"

#include <algorithm>

namespace {
bool is_utf8_continuation(char ch) {
    return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
}

std::size_t move_to_char_boundary(const std::string& text, std::size_t pos) {
    while (pos < text.size() && is_utf8_continuation(text[pos])) {
        ++pos;
    }
    return pos;
}
}

std::vector<DocumentChunk> split_documents(const std::vector<DocumentChunk>& docs, const Config& config) {
    std::vector<DocumentChunk> chunks;
    for (const auto& doc : docs) {
        if (doc.text.empty()) {
            continue;
        }

        std::size_t start = 0;
        while (start < doc.text.size()) {
            std::size_t end = std::min(start + config.chunk_size, doc.text.size());
            if (end < doc.text.size()) {
                std::size_t boundary = doc.text.rfind('\n', end);
                if (boundary != std::string::npos && boundary > start + config.chunk_size / 2) {
                    end = boundary + 1;
                }
            }
            end = move_to_char_boundary(doc.text, end);

            DocumentChunk chunk = doc;
            chunk.text = doc.text.substr(start, end - start);
            chunks.push_back(std::move(chunk));

            if (end >= doc.text.size()) {
                break;
            }
            start = end > config.chunk_overlap ? end - config.chunk_overlap : end;
            start = move_to_char_boundary(doc.text, start);
        }
    }
    return chunks;
}
