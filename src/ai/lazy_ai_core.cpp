#include "ai/lazy_ai_core.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>

namespace Blueprint::AI {

namespace {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* mem = static_cast<std::string*>(userp);
    mem->append(static_cast<char*>(contents), realsize);
    return realsize;
}

} // anonymous namespace

LazyAICore& LazyAICore::instance() {
    static LazyAICore core;
    return core;
}

LazyAICore::LazyAICore() {
    // Default state: INACTIVE_SHUTDOWN, 0 CPU / 0 Thread allocation
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LazyAICore::~LazyAICore() {
    shutdown();
    curl_global_cleanup();
}

void LazyAICore::configure(const AIConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    if (!m_config.enabled && m_state != AIState::INACTIVE_SHUTDOWN) {
        shutdown();
    }
}

bool LazyAICore::activate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != AIState::INACTIVE_SHUTDOWN) return true;

    m_state = AIState::INITIALIZING;
    m_stopWorker = false;
    m_state = AIState::READY;
    return true;
}

void LazyAICore::shutdown() {
    m_stopWorker = true;
    if (m_workerThread && m_workerThread->joinable()) {
        m_workerThread->join();
        m_workerThread.reset();
    }
    m_state = AIState::INACTIVE_SHUTDOWN;
}

std::string LazyAICore::executeCurlRequest(const std::string& url, const std::string& jsonPayload, const std::string& authHeader) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;

    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!authHeader.empty()) {
            headers = curl_slist_append(headers, authHeader.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "[AI Core] Curl failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

void LazyAICore::queryAsync(const std::string& prompt, const std::string& pageContext, AIResponseCallback callback) {
    if (m_state == AIState::INACTIVE_SHUTDOWN) {
        if (!activate()) {
            if (callback) callback(false, "AI Core is currently inactive");
            return;
        }
    }

    // Launch isolated std::thread so 144Hz compositor frame rate is never stuttered
    std::thread([this, prompt, pageContext, callback]() {
        m_state = AIState::PROCESSING;

        std::string url;
        std::string authHeader;
        std::string payload;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_config.provider == AIProvider::LOCAL_OLLAMA) {
                url = m_config.endpointUrl.empty() ? "http://localhost:11434/api/generate" : m_config.endpointUrl;
                payload = "{\"model\": \"" + m_config.modelName + "\", \"prompt\": \"" + prompt + "\", \"stream\": false}";
            } else if (m_config.provider == AIProvider::BYOK_OPENAI) {
                url = "https://api.openai.com/v1/chat/completions";
                authHeader = "Authorization: Bearer " + m_config.apiKey;
                payload = "{\"model\": \"gpt-4o-mini\", \"messages\": [{\"role\": \"user\", \"content\": \"" + prompt + "\"}]}";
            } else if (m_config.provider == AIProvider::BYOK_DEEPSEEK) {
                url = "https://api.deepseek.com/v1/chat/completions";
                authHeader = "Authorization: Bearer " + m_config.apiKey;
                payload = "{\"model\": \"deepseek-chat\", \"messages\": [{\"role\": \"user\", \"content\": \"" + prompt + "\"}]}";
            } else {
                url = m_config.endpointUrl;
            }
        }

        std::string result = executeCurlRequest(url, payload, authHeader);
        m_state = AIState::READY;

        if (callback) {
            bool success = !result.empty();
            callback(success, result.empty() ? "No response from AI provider" : result);
        }
    }).detach();
}

} // namespace Blueprint::AI
