#pragma once

#include "config.hpp"

#include <string>
#include <vector>

class OllamaClient {
public:
    explicit OllamaClient(Config config);

    std::vector<double> embed(const std::string& text) const;
    std::string generate(const std::string& prompt) const;

private:
    Config config_;
    std::string post_json(const std::string& path, const std::string& body) const;
};

