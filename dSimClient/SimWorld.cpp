#include "SimWorld.h"
#include "SimUser.h"
#include "SimConfig.h"
#include "Logger.h"
#include "Game.h"

#include <iostream>
#include <algorithm>
#include <thread>
#include <random>
#include <iomanip>

SimWorld::SimWorld()
    : m_NextUserID(1)
    , m_Running(false)
    , m_Initialized(false)
    , m_TotalUsersSpawned(0)
    , m_TotalUsersDisconnected(0)
    , m_TotalErrors(0)
{
    m_StartTime = std::chrono::steady_clock::now();
    m_LastSpawn = std::chrono::steady_clock::now();
    m_LastStatsPrint = std::chrono::steady_clock::now();
}

SimWorld::~SimWorld() {
    Shutdown();
}

bool SimWorld::Initialize(std::shared_ptr<SimConfig> config) {
    if (m_Initialized) {
        std::cout << "SimWorld already initialized" << std::endl;
        return false;
    }
    
    m_Config = config;
    m_Initialized = true;
    
    std::cout << "SimWorld initialized with configuration:" << std::endl;
    std::cout << "  Auth Server: " << m_Config->GetAuthServerIP() << ":" << m_Config->GetAuthServerPort() << std::endl;
    std::cout << "  World Server: " << m_Config->GetWorldServerIP() << ":" << m_Config->GetWorldServerPort() << std::endl;
    std::cout << "  Client Count: " << m_Config->GetClientCount() << std::endl;
    std::cout << "  Spawn Delay: " << m_Config->GetSpawnDelayMs() << "ms" << std::endl;
    std::cout << "  Tick Rate: " << m_Config->GetTickRateMs() << "ms" << std::endl;
    
    return true;
}

void SimWorld::Shutdown() {
    if (!m_Initialized) return;
    
    m_Running = false;
    
    // Disconnect all users
    std::cout << "Shutting down simulation, disconnecting " << m_Users.size() << " users..." << std::endl;
    
    for (auto& user : m_Users) {
        user->Disconnect();
    }
    
    m_Users.clear();
    m_Initialized = false;
    
    std::cout << "SimWorld shutdown complete" << std::endl;
}

void SimWorld::Run() {
    if (!m_Initialized) {
        std::cout << "SimWorld not initialized, cannot run" << std::endl;
        return;
    }
    
    m_Running = true;
    std::cout << "Starting simulation..." << std::endl;
    
    auto lastUpdate = std::chrono::steady_clock::now();
    const auto tickRate = std::chrono::milliseconds(m_Config->GetTickRateMs());
    
    while (m_Running) {
        auto now = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration<float>(now - lastUpdate).count();
        lastUpdate = now;
        
        Update(deltaTime);
        
        // Print statistics every 10 seconds
        auto timeSinceLastStats = std::chrono::duration_cast<std::chrono::seconds>(now - m_LastStatsPrint);
        if (timeSinceLastStats.count() >= 10) {
            PrintStatistics();
            m_LastStatsPrint = now;
        }
        
        // Sleep to maintain tick rate
        std::this_thread::sleep_for(tickRate);
    }
    
    std::cout << "Simulation stopped" << std::endl;
}

void SimWorld::Update(float deltaTime) {
    SpawnUsersGradually();
    CheckUserStates();
    HandleUserErrors();
    
    // Update all users
    for (auto& user : m_Users) {
        user->Update(deltaTime);
    }
}

void SimWorld::SpawnUsersGradually() {
    if (m_Users.size() >= m_Config->GetClientCount()) {
        return; // Already have enough users
    }
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastSpawn = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastSpawn);
    
    if (timeSinceLastSpawn.count() >= m_Config->GetSpawnDelayMs()) {
        // Get credentials from config or generate random ones
        std::string username, password;
        
        auto testAccounts = m_Config->GetTestAccounts();
        if (m_TotalUsersSpawned < testAccounts.size()) {
            // Use predefined test account
            username = testAccounts[m_TotalUsersSpawned].first;
            password = testAccounts[m_TotalUsersSpawned].second;
        } else {
            // Generate random credentials
            username = GenerateRandomUsername();
            password = GenerateRandomPassword();
        }
        
        if (SpawnUser(username, password)) {
            m_LastSpawn = now;
            m_TotalUsersSpawned++;
        }
    }
}

bool SimWorld::SpawnUser(const std::string& username, const std::string& password) {
    auto user = std::make_unique<SimUser>(m_NextUserID++, username, password);
    
    if (!user->ConnectToAuth(m_Config->GetAuthServerIP(), m_Config->GetAuthServerPort())) {
        std::cout << "Failed to connect user " << username << " to auth server" << std::endl;
        return false;
    }
    
    m_Users.push_back(std::move(user));
    
    if (m_Config->GetVerboseLogging()) {
        std::cout << "Spawned user: " << username << " (Total: " << m_Users.size() << ")" << std::endl;
    }
    
    return true;
}

void SimWorld::RemoveUser(uint32_t userID) {
    auto it = std::remove_if(m_Users.begin(), m_Users.end(),
        [userID](const std::unique_ptr<SimUser>& user) {
            return user->GetID() == userID;
        });
    
    if (it != m_Users.end()) {
        (*it)->Disconnect();
        m_Users.erase(it, m_Users.end());
        m_TotalUsersDisconnected++;
    }
}

void SimWorld::CheckUserStates() {
    // Remove users that have been in error state for too long
    auto it = std::remove_if(m_Users.begin(), m_Users.end(),
        [this](const std::unique_ptr<SimUser>& user) {
            if (user->GetState() == SimUserState::ERROR_STATE) {
                if (m_Config->GetVerboseLogging()) {
                    std::cout << "Removing user " << user->GetUsername() 
                              << " due to error: " << user->GetLastError() << std::endl;
                }
                m_TotalErrors++;
                return true;
            }
            return false;
        });
    
    if (it != m_Users.end()) {
        m_Users.erase(it, m_Users.end());
    }
}

void SimWorld::HandleUserErrors() {
    uint32_t errorCount = GetErrorUserCount();
    if (errorCount > 0 && m_Config->GetVerboseLogging()) {
        std::cout << "Warning: " << errorCount << " users in error state" << std::endl;
    }
}

void SimWorld::PrintStatistics() {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - m_StartTime);
    
    std::cout << std::endl;
    std::cout << "=== Simulation Statistics ===" << std::endl;
    std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
    std::cout << "Active Users: " << GetActiveUserCount() << std::endl;
    std::cout << "Connected Users: " << GetConnectedUserCount() << std::endl;
    std::cout << "Total Spawned: " << m_TotalUsersSpawned << std::endl;
    std::cout << "Total Disconnected: " << m_TotalUsersDisconnected << std::endl;
    std::cout << "Total Errors: " << m_TotalErrors << std::endl;
    std::cout << "Total Packets Sent: " << GetTotalPacketsSent() << std::endl;
    std::cout << "Total Packets Received: " << GetTotalPacketsReceived() << std::endl;
    std::cout << "Total Bytes Sent: " << GetTotalBytesSent() << std::endl;
    std::cout << "Total Bytes Received: " << GetTotalBytesReceived() << std::endl;
    
    if (uptime.count() > 0) {
        std::cout << "Avg Packets/sec Sent: " << (GetTotalPacketsSent() / uptime.count()) << std::endl;
        std::cout << "Avg Packets/sec Received: " << (GetTotalPacketsReceived() / uptime.count()) << std::endl;
        std::cout << "Avg Bytes/sec Sent: " << (GetTotalBytesSent() / uptime.count()) << std::endl;
        std::cout << "Avg Bytes/sec Received: " << (GetTotalBytesReceived() / uptime.count()) << std::endl;
    }
    std::cout << "=============================" << std::endl;
    std::cout << std::endl;
}

uint64_t SimWorld::GetTotalPacketsSent() const {
    uint64_t total = 0;
    for (const auto& user : m_Users) {
        total += user->GetPacketsSent();
    }
    return total;
}

uint64_t SimWorld::GetTotalPacketsReceived() const {
    uint64_t total = 0;
    for (const auto& user : m_Users) {
        total += user->GetPacketsReceived();
    }
    return total;
}

uint64_t SimWorld::GetTotalBytesSent() const {
    uint64_t total = 0;
    for (const auto& user : m_Users) {
        total += user->GetBytesSent();
    }
    return total;
}

uint64_t SimWorld::GetTotalBytesReceived() const {
    uint64_t total = 0;
    for (const auto& user : m_Users) {
        total += user->GetBytesReceived();
    }
    return total;
}

uint32_t SimWorld::GetConnectedUserCount() const {
    uint32_t count = 0;
    for (const auto& user : m_Users) {
        if (user->GetState() == SimUserState::IN_WORLD || 
            user->GetState() == SimUserState::LOADING_WORLD ||
            user->GetState() == SimUserState::CONNECTING_TO_WORLD ||
            user->GetState() == SimUserState::AUTHENTICATING) {
            count++;
        }
    }
    return count;
}

uint32_t SimWorld::GetErrorUserCount() const {
    uint32_t count = 0;
    for (const auto& user : m_Users) {
        if (user->GetState() == SimUserState::ERROR_STATE) {
            count++;
        }
    }
    return count;
}

std::string SimWorld::GenerateRandomUsername() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);
    
    return "simuser" + std::to_string(dis(gen));
}

std::string SimWorld::GenerateRandomPassword() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(10000, 99999);
    
    return "simpass" + std::to_string(dis(gen));
}

void SimWorld::RunLoadTest() {
    std::cout << "Running load test with " << m_Config->GetClientCount() << " clients..." << std::endl;
    Run();
}

void SimWorld::RunStressTest() {
    std::cout << "Running stress test..." << std::endl;
    
    // Temporarily increase client count for stress testing
    uint32_t originalCount = m_Config->GetClientCount();
    m_Config->SetClientCount(originalCount * 2);
    
    Run();
    
    // Restore original count
    m_Config->SetClientCount(originalCount);
}

void SimWorld::RunBasicConnectivityTest() {
    std::cout << "Running basic connectivity test..." << std::endl;
    
    // Set to 1 client for basic test
    uint32_t originalCount = m_Config->GetClientCount();
    m_Config->SetClientCount(1);
    
    Run();
    
    // Restore original count
    m_Config->SetClientCount(originalCount);
}

void SimWorld::RunMovementTest() {
    std::cout << "Running movement test..." << std::endl;
    // Movement is handled automatically in the user simulation
    Run();
}

void SimWorld::RunChatTest() {
    std::cout << "Running chat test..." << std::endl;
    // Chat is handled automatically in the user simulation
    Run();
}
