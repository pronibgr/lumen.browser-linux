#include "core/tor_provisioner.hpp"
#include "core/tor_bridge.hpp"
#include <glib.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace Blueprint::Core {

TorProvisioner& TorProvisioner::instance() {
    static TorProvisioner s_inst;
    return s_inst;
}

TorProvisioner::~TorProvisioner() {
    // allow worker to complete cleanly if running
}

bool TorProvisioner::isRunning() const {
    return m_running.load();
}

ProvisionState TorProvisioner::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

std::string TorProvisioner::getStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statusMessage;
}

std::vector<std::string> TorProvisioner::getLogs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_logs;
}

void TorProvisioner::clearLogs() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logs.clear();
    m_state = ProvisionState::IDLE;
    m_statusMessage.clear();
}

void TorProvisioner::setOnLog(std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onLog = cb;
}

void TorProvisioner::setOnStatusChanged(std::function<void(ProvisionState, const std::string&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onStatusChanged = cb;
}

void TorProvisioner::appendLog(const std::string& line) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.push_back(line);
    }
    struct LogEvent {
        TorProvisioner* self;
        std::string line;
    };
    auto* ev = new LogEvent{this, line};
    g_idle_add(+[](gpointer d) -> gboolean {
        auto* e = static_cast<LogEvent*>(d);
        std::function<void(const std::string&)> cb;
        {
            std::lock_guard<std::mutex> lock(e->self->m_mutex);
            cb = e->self->m_onLog;
        }
        if (cb) {
            cb(e->line);
        }
        delete e;
        return G_SOURCE_REMOVE;
    }, ev);
}

void TorProvisioner::setStatus(ProvisionState state, const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = state;
        m_statusMessage = msg;
    }
    struct StatusEvent {
        TorProvisioner* self;
        ProvisionState state;
        std::string msg;
    };
    auto* ev = new StatusEvent{this, state, msg};
    g_idle_add(+[](gpointer d) -> gboolean {
        auto* e = static_cast<StatusEvent*>(d);
        std::function<void(ProvisionState, const std::string&)> cb;
        {
            std::lock_guard<std::mutex> lock(e->self->m_mutex);
            cb = e->self->m_onStatusChanged;
        }
        if (cb) {
            cb(e->state, e->msg);
        }
        delete e;
        return G_SOURCE_REMOVE;
    }, ev);
}

std::string TorProvisioner::detectInstallCommand() {
    // 1. Inspect /etc/os-release for distro identity
    std::string osRelease;
    std::ifstream f("/etc/os-release");
    if (f.is_open()) {
        std::stringstream ss;
        ss << f.rdbuf();
        osRelease = ss.str();
    }

    auto hasBinary = [](const char* name) -> bool {
        char* p = g_find_program_in_path(name);
        if (p) {
            g_free(p);
            return true;
        }
        return false;
    };

    // Arch / CachyOS / Manjaro
    if (hasBinary("pacman") || osRelease.find("ID=arch") != std::string::npos ||
        osRelease.find("ID=cachyos") != std::string::npos || osRelease.find("ID=manjaro") != std::string::npos ||
        osRelease.find("ID_LIKE=arch") != std::string::npos) {
        return "pacman -S --noconfirm --needed tor && systemctl enable --now tor";
    }

    // Debian / Ubuntu / Linux Mint
    if (hasBinary("apt-get") || osRelease.find("ID=debian") != std::string::npos ||
        osRelease.find("ID=ubuntu") != std::string::npos || osRelease.find("ID_LIKE=debian") != std::string::npos) {
        return "apt-get update && apt-get install -y tor && systemctl enable --now tor";
    }

    // Fedora / RHEL
    if (hasBinary("dnf") || osRelease.find("ID=fedora") != std::string::npos ||
        osRelease.find("ID=rhel") != std::string::npos) {
        return "dnf install -y tor && systemctl enable --now tor";
    }

    // openSUSE
    if (hasBinary("zypper") || osRelease.find("ID=opensuse") != std::string::npos) {
        return "zypper --non-interactive in tor && systemctl enable --now tor";
    }

    // Default fallback
    if (hasBinary("pacman")) {
        return "pacman -S --noconfirm --needed tor && systemctl enable --now tor";
    }
    return "apt-get update && apt-get install -y tor && systemctl enable --now tor";
}

void TorProvisioner::checkOrProvision(int port, bool autoInstall) {
    if (m_running.exchange(true)) {
        return; // already executing
    }

    setStatus(ProvisionState::RUNNING, "Probing Tor daemon and system services...");
    appendLog("=== Starting Tor Daemon Verification (Port " + std::to_string(port) + ") ===");

    std::thread([this, port, autoInstall]() {
        // Step 1: Read-Only Verification Phase
        bool socketOk = TorBridge::probeTorDaemon(port, 400);

        char* torPath = g_find_program_in_path("tor");
        std::string binaryPath = torPath ? torPath : "";
        if (torPath) g_free(torPath);

        bool binaryFound = !binaryPath.empty();
        if (binaryFound) {
            appendLog("[OK] Tor binary located: " + binaryPath);
        } else {
            appendLog("[INFO] Tor binary not found in system PATH.");
        }

        // Check systemctl unit status
        bool unitActive = (system("systemctl is-active --quiet tor 2>/dev/null") == 0);
        if (unitActive) {
            appendLog("[OK] systemd unit 'tor.service' is reported active.");
        } else {
            appendLog("[INFO] systemd unit 'tor.service' is inactive or uninstalled.");
        }

        if (socketOk) {
            appendLog("[OK] SOCKS5 probe succeeded: 127.0.0.1:" + std::to_string(port) + " is active and responding.");
            setStatus(ProvisionState::SUCCESS, "Tor service is installed, configured, and running properly.");
            m_running.store(false);
            return;
        }

        appendLog("[WARN] SOCKS5 probe failed: 127.0.0.1:" + std::to_string(port) + " is unreachable or not responding.");

        if (!autoInstall) {
            setStatus(ProvisionState::ERROR, "Tor service is inactive or not responding on 127.0.0.1:" + std::to_string(port));
            m_running.store(false);
            return;
        }

        // Step 2: Auto-Provisioning & Installation Phase
        appendLog("[INFO] Tor daemon is unreachable. Preparing system provisioning...");
        std::string installCmd = detectInstallCommand();
        appendLog("[CMD] Selected installer: " + installCmd);
        appendLog("[AUTH] Invoking PolicyKit (pkexec) for administrative elevation...");

        std::string fullCmd = "pkexec bash -c \"" + installCmd + "\" 2>&1";

        FILE* pipe = popen(fullCmd.c_str(), "r");
        if (!pipe) {
            appendLog("[FAIL] Failed to launch pkexec child process.");
            setStatus(ProvisionState::ERROR, "Failed to launch PolicyKit elevation wrapper.");
            m_running.store(false);
            return;
        }

        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            std::string line = buffer;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            if (!line.empty()) {
                appendLog(line);
            }
        }

        int pcloseStatus = pclose(pipe);
        int exitCode = WIFEXITED(pcloseStatus) ? WEXITSTATUS(pcloseStatus) : -1;

        if (exitCode == 126 || exitCode == 127) {
            appendLog("[FAIL] PolicyKit authentication was dismissed or cancelled by user.");
            setStatus(ProvisionState::ERROR, "Polkit authentication dismissed or cancelled by user.");
            m_running.store(false);
            return;
        }

        if (exitCode != 0) {
            appendLog("[FAIL] Installer process exited with non-zero status: " + std::to_string(exitCode));
            setStatus(ProvisionState::ERROR, "Package manager or systemd failed (code " + std::to_string(exitCode) + ").");
            m_running.store(false);
            return;
        }

        appendLog("[OK] Installation command completed successfully (code 0).");
        appendLog("[INFO] Polling 127.0.0.1:" + std::to_string(port) + " for active Tor listener...");

        // Step 3: Post-Provisioning Verification Loop (up to 3 seconds)
        bool verified = false;
        for (int attempt = 1; attempt <= 15; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (TorBridge::probeTorDaemon(port, 150)) {
                verified = true;
                break;
            }
        }

        if (verified) {
            appendLog("[OK] Verified: Tor daemon is active and listening on 127.0.0.1:" + std::to_string(port));
            setStatus(ProvisionState::SUCCESS, "Tor service is installed, configured, and running properly.");
        } else {
            appendLog("[WARN] Daemon was started, but 127.0.0.1:" + std::to_string(port) + " is not accepting connections yet.");
            setStatus(ProvisionState::ERROR, "Tor service started but 127.0.0.1:" + std::to_string(port) + " is not responding.");
        }

        m_running.store(false);
    }).detach();
}

} // namespace Blueprint::Core
