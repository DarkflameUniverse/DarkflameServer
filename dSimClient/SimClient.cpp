#include <iostream>
#include <memory>
#include <string>
#include <csignal>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// DLU Includes
#include "Logger.h"
#include "dConfig.h"
#include "Game.h"
#include "Server.h"
#include "BinaryPathFinder.h"

// SimClient includes
#include "SimConfig.h"
#include "SimWorld.h"
#include "SimUser.h"
#include "SimBehavior.h"

namespace Game {
    Logger* logger = nullptr;
    dConfig* config = nullptr;
    Game::signal_t lastSignal = 0;
}

void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help              Show this help message" << std::endl;
    std::cout << "  -c, --config <file>     Configuration file (default: simclient.ini)" << std::endl;
    std::cout << "  -n, --clients <count>   Number of clients to simulate (default: 1)" << std::endl;
    std::cout << "  -a, --auth <ip:port>    Auth server address (default: 127.0.0.1:1001)" << std::endl;
    std::cout << "  -w, --world <ip:port>   World server address (default: 127.0.0.1:2000)" << std::endl;
    std::cout << "  -v, --verbose           Enable verbose logging" << std::endl;
    std::cout << "  -t, --test <type>       Run specific test type:" << std::endl;
    std::cout << "                          basic     - Basic connectivity test (1 client)" << std::endl;
    std::cout << "                          load      - Load test (multiple clients)" << std::endl;
    std::cout << "                          stress    - Stress test (high load)" << std::endl;
    std::cout << "                          movement  - Movement simulation test" << std::endl;
    std::cout << "                          chat      - Chat functionality test" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " -n 10 -v                    # 10 clients with verbose logging" << std::endl;
    std::cout << "  " << programName << " -t load -n 50               # Load test with 50 clients" << std::endl;
    std::cout << "  " << programName << " -a 192.168.1.100:1001       # Connect to remote server" << std::endl;
}

void SignalHandler(int signal) {
    std::cout << std::endl << "Received signal " << signal << ", shutting down..." << std::endl;
    Game::lastSignal = signal;
}

bool ParseAddress(const std::string& addr, std::string& ip, uint16_t& port) {
    size_t colonPos = addr.find(':');
    if (colonPos == std::string::npos) {
        return false;
    }
    
    ip = addr.substr(0, colonPos);
    try {
        port = static_cast<uint16_t>(std::stoul(addr.substr(colonPos + 1)));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "DarkflameServer Simulation Client" << std::endl;
    std::cout << "Version: " << PROJECT_VERSION << std::endl;
    std::cout << "Compiled on: " << __TIMESTAMP__ << std::endl;
    std::cout << std::endl;

    // Set up signal handling
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // Parse command line arguments
    std::string configFile = "simclient.ini";
    std::string testType = "load";
    bool showHelp = false;
    
    auto config = std::make_shared<SimConfig>();
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            showHelp = true;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a filename" << std::endl;
                return 1;
            }
        } else if (arg == "-n" || arg == "--clients") {
            if (i + 1 < argc) {
                try {
                    uint32_t clientCount = std::stoul(argv[++i]);
                    config->SetClientCount(clientCount);
                } catch (const std::exception&) {
                    std::cerr << "Error: Invalid client count" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: " << arg << " requires a number" << std::endl;
                return 1;
            }
        } else if (arg == "-a" || arg == "--auth") {
            if (i + 1 < argc) {
                std::string ip;
                uint16_t port;
                if (ParseAddress(argv[++i], ip, port)) {
                    config->SetAuthServer(ip, port);
                } else {
                    std::cerr << "Error: Invalid auth server address format (use ip:port)" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: " << arg << " requires an address" << std::endl;
                return 1;
            }
        } else if (arg == "-w" || arg == "--world") {
            if (i + 1 < argc) {
                std::string ip;
                uint16_t port;
                if (ParseAddress(argv[++i], ip, port)) {
                    config->SetWorldServer(ip, port);
                } else {
                    std::cerr << "Error: Invalid world server address format (use ip:port)" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: " << arg << " requires an address" << std::endl;
                return 1;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            config->SetVerboseLogging(true);
        } else if (arg == "-t" || arg == "--test") {
            if (i + 1 < argc) {
                testType = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires a test type" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            return 1;
        }
    }
    
    if (showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }

    // Setup logging
    const auto logsDir = BinaryPathFinder::GetBinaryDir() / "logs";
    if (!std::filesystem::exists(logsDir)) {
        std::filesystem::create_directories(logsDir);
    }
    
    std::string logPath = (logsDir / "SimClient").string() + "_" + std::to_string(time(nullptr)) + ".log";
    Game::logger = new Logger(logPath, true, config->GetVerboseLogging());
    
    if (Game::logger) {
        #ifdef _WIN32
        LOG("SimClient started with PID %lu", GetCurrentProcessId());
        #else
        LOG("SimClient started with PID %d", getpid());
        #endif
    }

    // Load configuration file if it exists
    if (!config->LoadFromFile(configFile)) {
        std::cout << "Using default configuration (config file not found or invalid)" << std::endl;
    }

    // Create and initialize simulation world
    SimWorld simWorld;
    if (!simWorld.Initialize(config)) {
        std::cerr << "Failed to initialize simulation world" << std::endl;
        return 1;
    }

    std::cout << "Starting simulation..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;

    // Run the appropriate test
    try {
        if (testType == "basic") {
            simWorld.RunBasicConnectivityTest();
        } else if (testType == "load") {
            simWorld.RunLoadTest();
        } else if (testType == "stress") {
            simWorld.RunStressTest();
        } else if (testType == "movement") {
            simWorld.RunMovementTest();
        } else if (testType == "chat") {
            simWorld.RunChatTest();
        } else {
            std::cerr << "Unknown test type: " << testType << std::endl;
            std::cerr << "Valid types: basic, load, stress, movement, chat" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during simulation: " << e.what() << std::endl;
        return 1;
    }

    // Clean shutdown
    simWorld.Shutdown();
    
    if (Game::logger) {
        LOG("SimClient shutting down");
        delete Game::logger;
        Game::logger = nullptr;
    }

    std::cout << "Simulation complete!" << std::endl;
    return 0;
}
