#pragma once

#include <string>
#include <vector>
#include <memory>

/**
 * @brief Configuration class for simulation client
 */
class SimConfig {
public:
    SimConfig();
    ~SimConfig();

    // Load configuration from file
    bool LoadFromFile(const std::string& filename);
    
    // Server connection settings
    std::string GetAuthServerIP() const { return m_AuthServerIP; }
    uint16_t GetAuthServerPort() const { return m_AuthServerPort; }
    std::string GetWorldServerIP() const { return m_WorldServerIP; }
    uint16_t GetWorldServerPort() const { return m_WorldServerPort; }
    
    // Client simulation settings
    uint32_t GetClientCount() const { return m_ClientCount; }
    uint32_t GetSpawnDelayMs() const { return m_SpawnDelayMs; }
    uint32_t GetTickRateMs() const { return m_TickRateMs; }
    
    // Test behavior settings
    bool GetEnableMovement() const { return m_EnableMovement; }
    bool GetEnableChat() const { return m_EnableChat; }
    bool GetEnableRandomActions() const { return m_EnableRandomActions; }
    uint32_t GetActionIntervalMs() const { return m_ActionIntervalMs; }
    
    // Authentication settings
    std::vector<std::pair<std::string, std::string>> GetTestAccounts() const { return m_TestAccounts; }
    
    // Logging settings
    bool GetVerboseLogging() const { return m_VerboseLogging; }
    std::string GetLogFile() const { return m_LogFile; }

    // Setters for programmatic configuration
    void SetAuthServer(const std::string& ip, uint16_t port) { m_AuthServerIP = ip; m_AuthServerPort = port; }
    void SetWorldServer(const std::string& ip, uint16_t port) { m_WorldServerIP = ip; m_WorldServerPort = port; }
    void SetClientCount(uint32_t count) { m_ClientCount = count; }
    void SetVerboseLogging(bool enabled) { m_VerboseLogging = enabled; }

private:
    // Server connection
    std::string m_AuthServerIP;
    uint16_t m_AuthServerPort;
    std::string m_WorldServerIP;
    uint16_t m_WorldServerPort;
    
    // Client simulation
    uint32_t m_ClientCount;
    uint32_t m_SpawnDelayMs;
    uint32_t m_TickRateMs;
    
    // Test behavior
    bool m_EnableMovement;
    bool m_EnableChat;
    bool m_EnableRandomActions;
    uint32_t m_ActionIntervalMs;
    
    // Authentication
    std::vector<std::pair<std::string, std::string>> m_TestAccounts;
    
    // Logging
    bool m_VerboseLogging;
    std::string m_LogFile;
};
