#pragma once

#include <string>
#include <vector>

std::string json_escape(const std::string& value);
std::string extract_json_string_field(const std::string& json, const std::string& field);
std::vector<double> extract_first_embedding(const std::string& json);

