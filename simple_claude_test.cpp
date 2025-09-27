#include <iostream>
#include <curl/curl.h>
#include <string>
#include <cstdlib>

// CURL response callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

int main() {
    // Get API key from environment
    const char* api_key = std::getenv("CLAUDE_API_KEY");
    if (!api_key) {
        std::cout << "❌ CLAUDE_API_KEY environment variable not set\n";
        return 1;
    }

    std::cout << "🧪 Testing Claude API with simple request\n";
    std::cout << "API Key length: " << std::string(api_key).length() << " characters\n";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cout << "❌ Failed to initialize CURL\n";
        return 1;
    }

    // Simple test request
    std::string request_body = R"({
        "model": "claude-3-haiku-20240307",
        "max_tokens": 100,
        "messages": [
            {
                "role": "user",
                "content": "Hello Claude! Please respond with 'API test successful' if you can see this message."
            }
        ]
    })";

    std::string response_body;

    // Set CURL options
    struct curl_slist* headers = nullptr;
    std::string auth_header = "x-api-key: " + std::string(api_key);
    std::string content_header = "Content-Type: application/json";
    std::string anthropic_version = "anthropic-version: 2023-06-01";

    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, content_header.c_str());
    headers = curl_slist_append(headers, anthropic_version.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CryptoClaude/1.0");

    // Make the API call
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cout << "❌ CURL error: " << curl_easy_strerror(res) << std::endl;
        return 1;
    }

    std::cout << "Response code: " << response_code << std::endl;
    std::cout << "Response body: " << response_body << std::endl;

    if (response_code == 200) {
        std::cout << "✅ Claude API connectivity successful!\n";
        return 0;
    } else {
        std::cout << "❌ Claude API error: HTTP " << response_code << std::endl;
        return 1;
    }
}