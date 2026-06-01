#include "json_utils.hpp"

#include <cctype>
#include <stdexcept>

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += ch; break;
        }
    }
    return out;
}

std::string extract_json_string_field(const std::string& json, const std::string& field) {
    const std::string key = "\"" + field + "\"";
    std::size_t pos = json.find(key);
    if (pos == std::string::npos) {
        throw std::runtime_error("Missing JSON field: " + field);
    }
    pos = json.find(':', pos + key.size());
    pos = json.find('"', pos);
    if (pos == std::string::npos) {
        throw std::runtime_error("JSON field is not a string: " + field);
    }
    ++pos;

    std::string out;
    while (pos < json.size()) {
        char ch = json[pos++];
        if (ch == '"') {
            return out;
        }
        if (ch == '\\' && pos < json.size()) {
            char escaped = json[pos++];
            switch (escaped) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                default: out += escaped; break;
            }
        } else {
            out += ch;
        }
    }
    throw std::runtime_error("Unterminated JSON string field: " + field);
}

std::vector<double> extract_first_embedding(const std::string& json) {
    std::size_t pos = json.find("\"embeddings\"");
    if (pos == std::string::npos) {
        pos = json.find("\"embedding\"");
    }
    if (pos == std::string::npos) {
        throw std::runtime_error("Missing embedding in Ollama response");
    }

    pos = json.find('[', pos);
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid embedding JSON");
    }

    // /api/embed returns [[...]], while older APIs may return [...].
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos < json.size() && json[pos] == '[') {
        ++pos;
    }

    std::vector<double> values;
    while (pos < json.size()) {
        while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ',')) {
            ++pos;
        }
        if (pos >= json.size() || json[pos] == ']') {
            break;
        }

        const char* start = json.c_str() + pos;
        char* end = nullptr;
        double value = std::strtod(start, &end);
        if (end == start) {
            throw std::runtime_error("Failed to parse embedding number");
        }
        values.push_back(value);
        pos = static_cast<std::size_t>(end - json.c_str());
    }

    if (values.empty()) {
        throw std::runtime_error("Embedding vector is empty");
    }
    return values;
}

