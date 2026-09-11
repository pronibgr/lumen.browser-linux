#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>

namespace Blueprint::Core {

enum class ProvisionState {
    IDLE = 0,
    RUNNING,
    SUCCESS,
    ERROR
};

struct ProvisionStatus {
    ProvisionState state = ProvisionState::IDLE;
    std::string message;
};

class TorProvisioner {
public:
    static TorProvisioner& instance();

    // Start verification or auto-installation in a non-blocking background thread
    void checkOrProvision(int port = 9050, bool autoInstall = true);

    bool isRunning() const;
    ProvisionState getState() const;
    std::string getStatusMessage() const;

    // Terminal log management
    std::vector<std::string> getLogs() const;
    void clearLogs();

    // Callbacks for live UI updates (always dispatched on GTK main loop)
    void setOnLog(std::function<void(const std::string& line)> cb);
    void setOnStatusChanged(std::function<void(ProvisionState state, const std::string& msg)> cb);

    void appendLog(const std::string& line);
    void setStatus(ProvisionState state, const std::string& msg);

    // Helper to detect distro package manager command
    static std::string detectInstallCommand();

private:
    TorProvisioner() = default;
    ~TorProvisioner();
    TorProvisioner(const TorProvisioner&) = delete;
    TorProvisioner& operator=(const TorProvisioner&) = delete;

    std::atomic<bool> m_running{false};
    ProvisionState m_state{ProvisionState::IDLE};
    std::string m_statusMessage;

    mutable std::mutex m_mutex;
    std::vector<std::string> m_logs;

    std::function<void(const std::string&)> m_onLog;
    std::function<void(ProvisionState, const std::string&)> m_onStatusChanged;
};

} // namespace Blueprint::Core
