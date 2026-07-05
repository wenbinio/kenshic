#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace kmp {

class GameServer; // KenshiMP.ServerLib — kept out of Core headers

// Runs the authoritative GameServer inside the game process on a background
// thread — the "listen server" that makes hosting peer-to-peer: the host
// player's game IS the server, and other players connect directly to it.
// No external KenshiMP.Server.exe required.
//
// Threading model: EVERYTHING GameServer-related happens on the server thread
// (Start → LoadWorld → tick loop → SaveWorld → Stop). This matters because
// GameServer::Start does blocking work (UPnP/COM discovery, firewall rule,
// socket bind) that must not stall the render thread, and because it keeps
// GameServer single-threaded exactly like the dedicated exe's main(). The
// game thread only reads atomics published here.
//
// The local player connects to it like any remote peer (loopback ENet); the
// server already promotes loopback peers to host (ConnectedPlayer.isLoopback).
class EmbeddedServer {
public:
    enum class State : uint8_t {
        Stopped,   // not running
        Starting,  // thread launched, GameServer::Start in progress
        Running,   // listening; clients may connect
        Failed,    // GameServer::Start failed (port in use, etc.)
    };

    static EmbeddedServer& Get();

    // Launch the server thread. Config is loaded from server.json next to the
    // game (created with defaults if missing — same behavior as the dedicated
    // exe); portOverride != 0 replaces the configured port. Returns false if
    // already running/starting. Completion is asynchronous — poll GetState().
    bool StartAsync(uint16_t portOverride = 0);

    // Request shutdown and join the thread. Saves the world, closes the ENet
    // host, removes the UPnP mapping. Safe to call from any state, including
    // repeatedly. Blocking (bounded by one tick + save time).
    void Stop();

    State    GetState() const { return m_state.load(std::memory_order_acquire); }
    bool     IsRunning() const { return GetState() == State::Running; }
    bool     IsActive() const { // Starting or Running — i.e. "hosting"
        State s = GetState();
        return s == State::Starting || s == State::Running;
    }
    uint16_t GetPort() const { return m_port.load(std::memory_order_acquire); }

    // Human-readable state for UI/commands.
    std::string Describe() const;

private:
    EmbeddedServer() = default;
    ~EmbeddedServer();
    EmbeddedServer(const EmbeddedServer&) = delete;
    EmbeddedServer& operator=(const EmbeddedServer&) = delete;

    void RunLoop(uint16_t portOverride);

    std::thread              m_thread;
    std::atomic<State>       m_state{State::Stopped};
    std::atomic<bool>        m_stopRequested{false};
    std::atomic<uint16_t>    m_port{0};
};

} // namespace kmp
