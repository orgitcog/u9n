// ion-daemon — Standalone cognitive kernel process
// Runs as a local sidecar on port 16634, serving the DTE cognitive
// architecture via a minimal HTTP interface that the Electron renderer
// connects to.
//
// SPDX-License-Identifier: MIT

#include <ion/shell.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
typedef int ssize_t;
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define CLOSE_SOCKET ::close
#endif

static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running.store(false);
}

// ─── Minimal HTTP Response ───────────────────────────────────────────────────
static std::string http_response(int code, const std::string& status,
                                  const std::string& body) {
    std::string resp;
    resp += "HTTP/1.1 " + std::to_string(code) + " " + status + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp += "Access-Control-Allow-Headers: Content-Type\r\n";
    resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;
    return resp;
}

// ─── JSON Helpers ────────────────────────────────────────────────────────────
static std::string jstr(const std::string& key, const std::string& val) {
    return "\"" + key + "\":\"" + val + "\"";
}
static std::string jnum(const std::string& key, double val) {
    std::ostringstream oss;
    oss << "\"" << key << "\":" << val;
    return oss.str();
}
static std::string jint(const std::string& key, int val) {
    return "\"" + key + "\":" + std::to_string(val);
}

// ─── Beat Phase Name (mirrors private EchobeatsEngine::phase_name) ───────────
static std::string beat_phase_str(ion::BeatPhase p) {
    switch (p) {
        case ion::BeatPhase::INTAKE:  return "INTAKE";
        case ion::BeatPhase::PROCESS: return "PROCESS";
        case ion::BeatPhase::OUTPUT:  return "OUTPUT";
        case ion::BeatPhase::REFLECT: return "REFLECT";
    }
    return "UNKNOWN";
}

// ─── Shell State Name (mirrors private IonShell::state_name) ─────────────────
static std::string shell_state_str(ion::ShellState s) {
    switch (s) {
        case ion::ShellState::UNINITIALIZED: return "UNINITIALIZED";
        case ion::ShellState::BOOTING:       return "BOOTING";
        case ion::ShellState::RUNNING:       return "RUNNING";
        case ion::ShellState::REFLECTING:    return "REFLECTING";
        case ion::ShellState::EVOLVING:      return "EVOLVING";
        case ion::ShellState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        case ion::ShellState::HALTED:        return "HALTED";
    }
    return "UNKNOWN";
}

// ─── Route Handler ───────────────────────────────────────────────────────────
// Uses the ACTUAL public API from the headers:
//   shell.state           — ShellState enum (public member)
//   shell.version         — std::string (public member)
//   shell.boot_start_ms   — uint64_t (public member)
//   shell.boot_end_ms     — uint64_t (public member)
//   shell.agent.self      — CoreSelf (public member of IonAgent)
//     .self_image()       — returns SelfImage struct
//     .ladder.stage_name()— returns string
//   shell.agent.echobeats — EchobeatsEngine (public member)
//     .current_step       — uint8_t (1-12)
//     .total_cycles       — uint32_t
//     .streams[i].phase   — BeatPhase enum
//   shell.agent.membranes — MembraneSystem (public member)
//     .nesting()          — returns nesting notation string
//     .computation_steps  — uint32_t
//   shell.telemetry.gestalt — GestaltField struct (public)
//   shell.nsh(expr)       — NSH/Scheme eval (public)
//   shell.run(N)          — run N ticks (public)
//   shell.summary()       — full state dump (public)
//   shell.reflect()       — trigger reflection (public)

static std::string handle_request(const std::string& method,
                                   const std::string& path,
                                   ion::IonShell& shell) {
    // CORS preflight
    if (method == "OPTIONS") {
        return http_response(204, "No Content", "");
    }

    // Health check
    if (path == "/health" || path == "/") {
        uint64_t uptime = 0;
        if (shell.boot_end_ms > shell.boot_start_ms) {
            uptime = shell.boot_end_ms - shell.boot_start_ms;
        }
        std::string body = "{";
        body += jstr("status", "ok") + ",";
        body += jstr("name", "ion-daemon") + ",";
        body += jstr("version", shell.version) + ",";
        body += jint("shell_state", static_cast<int>(shell.state)) + ",";
        body += jstr("shell_state_name", shell_state_str(shell.state)) + ",";
        body += jnum("boot_ms", static_cast<double>(uptime));
        body += "}";
        return http_response(200, "OK", body);
    }

    // Gestalt field (telemetry)
    if (path == "/gestalt") {
        shell.run(1);
        auto& g = shell.telemetry.gestalt;
        std::string body = "{";
        body += jnum("coherence", g.coherence) + ",";
        body += jnum("arousal", g.arousal) + ",";
        body += jnum("valence", g.valence) + ",";
        body += jnum("novelty", g.novelty) + ",";
        body += jnum("complexity", g.complexity) + ",";
        body += jnum("delta_coherence", g.delta_coherence) + ",";
        body += jnum("delta_arousal", g.delta_arousal) + ",";
        body += jnum("delta_valence", g.delta_valence);
        body += "}";
        return http_response(200, "OK", body);
    }

    // Ontogenetic stage
    if (path == "/stage") {
        auto img = shell.agent.self.self_image();
        std::string body = "{";
        body += jstr("stage", shell.agent.self.ladder.stage_name()) + ",";
        body += jnum("coherence", img.coherence) + ",";
        body += jint("active_genes", static_cast<int>(img.active_genes)) + ",";
        body += jint("total_memories", static_cast<int>(img.total_memories));
        body += "}";
        return http_response(200, "OK", body);
    }

    // Echobeats cycle status
    if (path == "/echobeats") {
        auto& eb = shell.agent.echobeats;
        std::string body = "{";
        body += jint("step", static_cast<int>(eb.current_step)) + ",";
        body += jint("cycle", static_cast<int>(eb.total_cycles)) + ",";
        body += jstr("phase", beat_phase_str(eb.streams[0].phase));
        body += "}";
        return http_response(200, "OK", body);
    }

    // Membrane status
    if (path == "/membrane") {
        auto& mem = shell.agent.membranes;
        std::string body = "{";
        body += jstr("nesting", mem.nesting()) + ",";
        body += jint("computation_steps", static_cast<int>(mem.computation_steps));
        body += "}";
        return http_response(200, "OK", body);
    }

    // NSH eval (Scheme expression)
    if (path.substr(0, 5) == "/eval") {
        auto qpos = path.find("expr=");
        if (qpos != std::string::npos) {
            std::string expr = path.substr(qpos + 5);
            for (auto& c : expr) { if (c == '+') c = ' '; }
            try {
                std::string result = shell.nsh(expr);
                std::string body = "{";
                body += jstr("expr", expr) + ",";
                body += jstr("result", result);
                body += "}";
                return http_response(200, "OK", body);
            } catch (const std::exception& e) {
                std::string body = "{" + jstr("error", e.what()) + "}";
                return http_response(500, "Internal Server Error", body);
            }
        }
        return http_response(400, "Bad Request",
                             "{\"error\":\"missing expr parameter\"}");
    }

    // Summary (full shell state dump)
    if (path == "/summary") {
        std::string s = shell.summary();
        std::string escaped;
        for (char c : s) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') continue;
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
        }
        std::string body = "{" + jstr("summary", escaped) + "}";
        return http_response(200, "OK", body);
    }

    return http_response(404, "Not Found", "{\"error\":\"not found\"}");
}

// ─── Parse HTTP Request Line ─────────────────────────────────────────────────
static bool parse_request_line(const std::string& data,
                                std::string& method, std::string& path) {
    auto sp1 = data.find(' ');
    if (sp1 == std::string::npos) return false;
    auto sp2 = data.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return false;
    method = data.substr(0, sp1);
    path = data.substr(sp1 + 1, sp2 - sp1 - 1);
    return true;
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    int port = 16634;
    std::string data_dir = ".";

    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) port = 16634;
    }
    if (argc > 2) {
        data_dir = argv[2];
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    // Initialize the cognitive shell
    ion::IonShell shell(data_dir);
    shell.boot();

    std::cout << "ion-daemon v" << shell.version << std::endl;
    std::cout << "  Matula: 2058449375152220505" << std::endl;
    std::cout << "  State: " << shell_state_str(shell.state) << std::endl;
    std::cout << "  Active patterns: "
              << shell.agent.self.self_image().active_genes << std::endl;
    std::cout << "  Listening on http://localhost:" << port << std::endl;

    // Create server socket
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << port << std::endl;
        CLOSE_SOCKET(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        CLOSE_SOCKET(server_fd);
        return 1;
    }

    while (g_running.load()) {
        shell.run(1);

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        SOCKET client_fd = accept(server_fd,
            reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd == INVALID_SOCKET) continue;

        char buf[4096];
        std::memset(buf, 0, sizeof(buf));
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            std::string data(buf, static_cast<size_t>(n));
            std::string method, path;
            if (parse_request_line(data, method, path)) {
                std::string response = handle_request(method, path, shell);
                send(client_fd, response.c_str(),
                     static_cast<int>(response.size()), 0);
            }
        }

        CLOSE_SOCKET(client_fd);
    }

    std::cout << "\n[ion-daemon] Shutting down..." << std::endl;
    shell.shutdown();
    CLOSE_SOCKET(server_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "[ion-daemon] Halted." << std::endl;
    return 0;
}
