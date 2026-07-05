#include "embedded_server.h"
#include "server.h"        // KenshiMP.ServerLib — GameServer
#include "kmp/config.h"    // ServerConfig
#include <spdlog/spdlog.h>
#include <chrono>

namespace kmp {

EmbeddedServer& EmbeddedServer::Get() {
    static EmbeddedServer instance;
    return instance;
}

EmbeddedServer::~EmbeddedServer() {
    // Static-storage destructor — may run inside DllMain(PROCESS_DETACH) under
    // the Windows loader lock. Joining a live thread there deadlocks (thread
    // exit needs DLL_THREAD_DETACH, which needs the loader lock we hold), so
    // signal stop and DETACH. The clean path is the explicit Stop() from
    // Core::Shutdown, which runs outside the loader lock and does join.
    m_stopRequested.store(true, std::memory_order_release);
    if (m_thread.joinable()) {
        m_thread.detach();
    }
}

bool EmbeddedServer::StartAsync(uint16_t portOverride) {
    State expected = State::Stopped;
    if (!m_state.compare_exchange_strong(expected, State::Starting,
                                         std::memory_order_acq_rel)) {
        // Also allow relaunch after a failed start
        expected = State::Failed;
        if (!m_state.compare_exchange_strong(expected, State::Starting,
                                             std::memory_order_acq_rel)) {
            spdlog::warn("EmbeddedServer: StartAsync ignored — already {}", Describe());
            return false;
        }
    }

    // A previous Failed run's thread has already exited; reap it before reuse.
    if (m_thread.joinable()) m_thread.join();

    m_stopRequested.store(false, std::memory_order_release);
    m_thread = std::thread(&EmbeddedServer::RunLoop, this, portOverride);
    spdlog::info("EmbeddedServer: launch requested (port override: {})",
                 portOverride ? std::to_string(portOverride) : "none");
    return true;
}

void EmbeddedServer::Stop() {
    m_stopRequested.store(true, std::memory_order_release);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    // RunLoop sets Stopped/Failed on exit; force Stopped if we never launched.
    State s = m_state.load(std::memory_order_acquire);
    if (s == State::Starting || s == State::Running) {
        m_state.store(State::Stopped, std::memory_order_release);
    }
}

std::string EmbeddedServer::Describe() const {
    switch (GetState()) {
        case State::Stopped:  return "stopped";
        case State::Starting: return "starting";
        case State::Running:  return "running on port " + std::to_string(GetPort());
        case State::Failed:   return "failed to start";
    }
    return "unknown";
}

void EmbeddedServer::RunLoop(uint16_t portOverride) {
    // Mirror the dedicated exe's main(): load server.json (or create defaults),
    // Start, LoadWorld, fixed-rate tick loop, then SaveWorld + Stop.
    ServerConfig config;
    const std::string configPath = "server.json"; // cwd = Kenshi directory
    if (config.Load(configPath)) {
        spdlog::info("EmbeddedServer: loaded config from {}", configPath);
    } else {
        config.Save(configPath);
        spdlog::info("EmbeddedServer: no config at {}, defaults saved", configPath);
    }
    if (portOverride != 0) config.port = portOverride;
    m_port.store(config.port, std::memory_order_release);

    GameServer server;
    if (!server.Start(config)) {
        spdlog::error("EmbeddedServer: GameServer::Start failed (port {} in use? "
                      "another server running?)", config.port);
        m_state.store(State::Failed, std::memory_order_release);
        return;
    }

    server.LoadWorld();

    m_state.store(State::Running, std::memory_order_release);
    spdlog::info("EmbeddedServer: '{}' listening on port {} (max {} players, "
                 "tick {} Hz) — P2P host mode",
                 config.serverName, config.port, config.maxPlayers, config.tickRate);

    const int tickIntervalMs = (config.tickRate > 0) ? (1000 / config.tickRate) : 50;
    auto lastTick = std::chrono::steady_clock::now();

    while (!m_stopRequested.load(std::memory_order_acquire)) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick);

        if (elapsed.count() >= tickIntervalMs) {
            float deltaTime = elapsed.count() / 1000.f;
            lastTick = now;
            server.Update(deltaTime);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    spdlog::info("EmbeddedServer: stopping — saving world...");
    server.SaveWorld();
    server.Stop();
    m_state.store(State::Stopped, std::memory_order_release);
    spdlog::info("EmbeddedServer: stopped.");
}

} // namespace kmp
