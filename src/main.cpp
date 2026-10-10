#include <iostream>
#include <string>
#include <memory>
#include <csignal>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "utils/Logger.h"
#include "utils/NetworkUtils.h"
#include "backend/IMediaBackend.h"
#include "backend/MockMediaBackend.h"
#include "server/WebSocketServer.h"

#if defined(_WIN32)
#include "backend/WindowsMediaBackend.h"
#include <windows.h>
#elif defined(__linux__)
#include "backend/LinuxMediaBackend.h"
#endif

using namespace musicpad;

static std::atomic<bool> g_running{true};
static std::mutex g_cvMutex;
static std::condition_variable g_cv;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        utils::Logger::info("Received shutdown signal. Stopping...");
        g_running = false;
        g_cv.notify_all();
    }
}

#if defined(_WIN32)
BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        utils::Logger::info("Console close requested. Stopping...");
        g_running = false;
        g_cv.notify_all();
        return TRUE;
    }
    return FALSE;
}
#endif

void printUsage(const char* progName) {
    std::cout << "MusicPad Daemon - Remote Media Controller\n\n"
              << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  -p, --port <port>    WebSocket listening port (default: 51234)\n"
              << "  -m, --mock           Use mock media player backend for testing\n"
              << "  -h, --help           Show this help message\n\n";
}

int main(int argc, char* argv[]) {
    int port = 51234;
    bool useMock = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "-m" || arg == "--mock") {
            useMock = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#if defined(_WIN32)
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif

    std::cout << "\033[1;35m"
              << "========================================\n"
              << "          MusicPad C++ Daemon           \n"
              << "========================================\n"
              << "\033[0m";

    std::shared_ptr<backend::IMediaBackend> backend;

    if (useMock) {
        utils::Logger::info("Running with MockMediaBackend (simulation mode)");
        backend = std::make_shared<backend::MockMediaBackend>();
    } else {
#if defined(_WIN32)
        utils::Logger::info("Using WindowsMediaBackend (GSMTC & WASAPI)");
        backend = std::make_shared<backend::WindowsMediaBackend>();
#elif defined(__linux__)
        utils::Logger::info("Using LinuxMediaBackend (MPRIS2)");
        backend = std::make_shared<backend::LinuxMediaBackend>();
#else
        utils::Logger::warn("Unsupported OS. Falling back to MockMediaBackend");
        backend = std::make_shared<backend::MockMediaBackend>();
#endif
    }

    if (!backend->init()) {
        utils::Logger::error("Failed to initialize media backend.");
        return 1;
    }

    server::WebSocketServer wsServer(port, backend);
    if (!wsServer.start()) {
        utils::Logger::error("Failed to start WebSocket server.");
        backend->shutdown();
        return 1;
    }

    auto ipAddresses = utils::getLocalIpAddresses();
    std::cout << "\n\033[1;36mConnect from your phone using one of these addresses:\033[0m\n";
    for (const auto& ip : ipAddresses) {
        std::cout << "  -> \033[1;32mws://" << ip << ":" << port << "\033[0m\n";
    }
    std::cout << "\n\033[90mPress Ctrl+C to exit.\033[0m\n\n";

    // Wait for termination signal
    std::unique_lock<std::mutex> lock(g_cvMutex);
    g_cv.wait(lock, [] { return !g_running.load(); });

    utils::Logger::info("Shutting down...");
    wsServer.stop();
    backend->shutdown();
    utils::Logger::info("Daemon stopped cleanly.");

    return 0;
}
