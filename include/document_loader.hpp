#pragma once

#include "document.hpp"

#include <filesystem>
#include <vector>

std::vector<std::filesystem::path> list_supported_files(const std::filesystem::path& root);
std::vector<DocumentChunk> load_documents(const std::filesystem::path& root);

