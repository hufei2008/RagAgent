#pragma once

#include "config.hpp"
#include "document.hpp"

#include <vector>

std::vector<DocumentChunk> split_documents(const std::vector<DocumentChunk>& docs, const Config& config);

