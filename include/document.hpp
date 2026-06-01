#pragma once

#include <string>

struct DocumentChunk {
    std::string source;
    std::string file_name;
    int page = -1;
    std::string text;
};

