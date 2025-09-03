#pragma once

#include <memory>
#include <vector>
#include <string>
#include <chrono>

class SimUser;
class SimConfig;

/**
 * @brief Manages the simulation of multiple users and test behaviors
 */
class SimWorld {
public:
    SimWorld();
    ~SimWorld();

    // Initialization
    bool Initialize(std::shared_ptr<SimConfig> config);
    void Shutdown();
    
    // Main simulation loop
    void Run();
    void Update(float deltaTime);
    
    // User management
    bool SpawnUser(const std::string& username, const std::string& password);
    void RemoveUser(uint32_t userID);
    size_t GetActiveUserCount() const { return m_Users.size(); }
    
    // Test scenarios
    void RunLoadTest();
    void RunStressTest();
    void RunBasicConnectivityTest();
    void RunMovementTest();
    void RunChatTest();
    
    // Statistics
    void PrintStatistics();
    uint64_t GetTotalPacketsSent() const;
    uint64_t GetTotalPacketsReceived() const;
    uint64_t GetTotalBytesSent() const;
    uint64_t GetTotalBytesReceived() const;
    uint32_t GetConnectedUserCount() const;
    uint32_t GetErrorUserCount() const;
    
    // Control
    void StopSimulation() { m_Running = false; }
    bool IsRunning() const { return m_Running; }

private:
    // Helper methods
    void SpawnUsersGradually();
    void CheckUserStates();
    void HandleUserErrors();
    std::string GenerateRandomUsername();
    std::string GenerateRandomPassword();
    
    // Member variables
    std::shared_ptr<SimConfig> m_Config;
    std::vector<std::unique_ptr<SimUser>> m_Users;
    uint32_t m_NextUserID;
    bool m_Running;
    bool m_Initialized;
    
    // Timing
    std::chrono::steady_clock::time_point m_LastSpawn;
    std::chrono::steady_clock::time_point m_StartTime;
    std::chrono::steady_clock::time_point m_LastStatsPrint;
    
    // Statistics
    uint32_t m_TotalUsersSpawned;
    uint32_t m_TotalUsersDisconnected;
    uint32_t m_TotalErrors;
};
