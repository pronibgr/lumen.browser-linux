#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <memory>

namespace Blueprint::AI {

enum class AIState {
    INACTIVE_SHUTDOWN, // Default zero-overhead state
    INITIALIZING,
    READY,
    PROCESSING,
    ERROR
};

enum class AIProvider {
    BYOK_OPENAI,
    BYOK_GEMINI,
    BYOK_ANTHROPIC,
    BYOK_DEEPSEEK,
    LOCAL_OLLAMA
};

struct AIConfig {
    bool enabled = false;
    AIProvider provider = AIProvider::LOCAL_OLLAMA;
    std::string apiKey;
    std::string modelName = "llama3";
    std::string endpointUrl = "http://localhost:11434/api/generate";
};

using AIResponseCallback = std::function<void(bool success, const std::string& reply)>;

class LazyAICore {
public:
    static LazyAICore& instance();

    void configure(const AIConfig& config);
    bool activate();
    void shutdown();

    AIState getState() const { return m_state; }
    bool isActive() const { return m_state != AIState::INACTIVE_SHUTDOWN; }

    // Async thread-isolated prompt query
    void queryAsync(const std::string& prompt, const std::string& pageContext, AIResponseCallback callback);

private:
    LazyAICore();
    ~LazyAICore();
    LazyAICore(const LazyAICore&) = delete;
    LazyAICore& operator=(const LazyAICore&) = delete;

    std::atomic<AIState> m_state{AIState::INACTIVE_SHUTDOWN};
    AIConfig m_config;
    std::mutex m_mutex;

    // Background worker thread
    std::unique_ptr<std::thread> m_workerThread;
    std::atomic<bool> m_stopWorker{false};

    void workerLoop();
    std::string executeCurlRequest(const std::string& url, const std::string& jsonPayload, const std::string& authHeader);
};

} // namespace Blueprint::AI
