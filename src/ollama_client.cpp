#include "ollama_client.hpp"

#include "json_utils.hpp"

#include <curl/curl.h>

#include <stdexcept>

namespace {
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}
}

OllamaClient::OllamaClient(Config config) : config_(std::move(config)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::string OllamaClient::post_json(const std::string& path, const std::string& body) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }

    std::string response;
    std::string url = config_.ollama_base_url + path;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);

    CURLcode code = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        throw std::runtime_error(std::string("Ollama request failed: ") + curl_easy_strerror(code));
    }
    if (http_code < 200 || http_code >= 300) {
        throw std::runtime_error("Ollama HTTP error " + std::to_string(http_code) + ": " + response);
    }
    return response;
}

std::vector<double> OllamaClient::embed(const std::string& text) const {
    std::string body = "{\"model\":\"" + json_escape(config_.embedding_model) +
        "\",\"input\":\"" + json_escape(text) + "\"}";
    return extract_first_embedding(post_json("/api/embed", body));
}

std::string OllamaClient::generate(const std::string& prompt) const {
    std::string body = "{\"model\":\"" + json_escape(config_.llm_model) +
        "\",\"prompt\":\"" + json_escape(prompt) +
        "\",\"stream\":false,\"options\":{\"temperature\":0.2}}";
    return extract_json_string_field(post_json("/api/generate", body), "response");
}

