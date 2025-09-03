#include "SimConfig.h"
#include <fstream>
#include <sstream>
#include <iostream>

SimConfig::SimConfig() 
    : m_AuthServerIP("127.0.0.1")
    , m_AuthServerPort(1001)
    , m_WorldServerIP("127.0.0.1") 
    , m_WorldServerPort(2000)
    , m_ClientCount(1)
    , m_SpawnDelayMs(1000)
    , m_TickRateMs(16)
    , m_EnableMovement(true)
    , m_EnableChat(false)
    , m_EnableRandomActions(true)
    , m_ActionIntervalMs(5000)
    , m_VerboseLogging(false)
    , m_LogFile("simclient.log")
{
    // Add some default test accounts
    m_TestAccounts.push_back(std::make_pair("testuser1", "testpass1"));
    m_TestAccounts.push_back(std::make_pair("testuser2", "testpass2"));
    m_TestAccounts.push_back(std::make_pair("testuser3", "testpass3"));
    m_TestAccounts.push_back(std::make_pair("testuser4", "testpass4"));
    m_TestAccounts.push_back(std::make_pair("testuser5", "testpass5"));
}

SimConfig::~SimConfig() {
}

bool SimConfig::LoadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Warning: Could not open config file '" << filename << "', using defaults" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Parse configuration values
        if (key == "auth_server_ip") {
            m_AuthServerIP = value;
        } else if (key == "auth_server_port") {
            m_AuthServerPort = static_cast<uint16_t>(std::stoul(value));
        } else if (key == "world_server_ip") {
            m_WorldServerIP = value;
        } else if (key == "world_server_port") {
            m_WorldServerPort = static_cast<uint16_t>(std::stoul(value));
        } else if (key == "client_count") {
            m_ClientCount = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "spawn_delay_ms") {
            m_SpawnDelayMs = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "tick_rate_ms") {
            m_TickRateMs = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "enable_movement") {
            m_EnableMovement = (value == "1" || value == "true");
        } else if (key == "enable_chat") {
            m_EnableChat = (value == "1" || value == "true");
        } else if (key == "enable_random_actions") {
            m_EnableRandomActions = (value == "1" || value == "true");
        } else if (key == "action_interval_ms") {
            m_ActionIntervalMs = static_cast<uint32_t>(std::stoul(value));
        } else if (key == "verbose_logging") {
            m_VerboseLogging = (value == "1" || value == "true");
        } else if (key == "log_file") {
            m_LogFile = value;
        } else if (key.substr(0, 12) == "test_account") {
            // Parse test_account_1=username:password format
            size_t colonPos = value.find(':');
            if (colonPos != std::string::npos) {
                std::string username = value.substr(0, colonPos);
                std::string password = value.substr(colonPos + 1);
                
                // Check if this account already exists
                bool found = false;
                for (auto& account : m_TestAccounts) {
                    if (account.first == username) {
                        account.second = password;
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    m_TestAccounts.push_back(std::make_pair(username, password));
                }
            }
        }
    }

    file.close();
    return true;
}
