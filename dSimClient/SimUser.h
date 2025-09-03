#pragma once

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "RakPeerInterface.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include <string>
#include <chrono>
#include <random>

enum class SimUserState {
    DISCONNECTED,
    CONNECTING_TO_AUTH,
    AUTHENTICATING,
    CONNECTING_TO_WORLD,
    LOADING_WORLD,
    IN_WORLD,
    DISCONNECTING,
    ERROR_STATE
};

/**
 * @brief Represents a simulated user/client connection
 */
class SimUser {
public:
    SimUser(uint32_t id, const std::string& username, const std::string& password);
    ~SimUser();

    // Connection management
    bool ConnectToAuth(const std::string& authIP, uint16_t authPort);
    bool ConnectToWorld(const std::string& worldIP, uint16_t worldPort);
    void Disconnect();
    
    // State management
    SimUserState GetState() const { return m_State; }
    void SetState(SimUserState state) { m_State = state; }
    
    // User info
    uint32_t GetID() const { return m_ID; }
    const std::string& GetUsername() const { return m_Username; }
    const std::string& GetPassword() const { return m_Password; }
    
    // Network handling
    void ProcessIncomingPackets();
    void SendHeartbeat();
    void Update(float deltaTime);
    
    // Simulation behaviors
    void SimulateMovement();
    void SimulateRandomAction();
    void SendChatMessage(const std::string& message);
    
    // Position and movement
    void SetPosition(const NiPoint3& position) { m_Position = position; }
    void SetRotation(const NiQuaternion& rotation) { m_Rotation = rotation; }
    const NiPoint3& GetPosition() const { return m_Position; }
    const NiQuaternion& GetRotation() const { return m_Rotation; }
    
    // Statistics
    uint64_t GetPacketsSent() const { return m_PacketsSent; }
    uint64_t GetPacketsReceived() const { return m_PacketsReceived; }
    uint64_t GetBytesReceived() const { return m_BytesReceived; }
    uint64_t GetBytesSent() const { return m_BytesSent; }
    
    // Error handling
    bool HasError() const { return m_HasError; }
    const std::string& GetLastError() const { return m_LastError; }

private:
    // Helper methods
    void HandleAuthPacket(Packet* packet);
    void HandleWorldPacket(Packet* packet);
    void SetError(const std::string& error);
    void LogMessage(const std::string& message);
    
    // Authentication flow
    void SendLoginRequest();
    void HandleLoginResponse(Packet* packet);
    void HandleWorldServerInfo(Packet* packet);
    
    // World flow  
    void SendWorldLoginRequest();
    void HandleWorldLoginResponse(Packet* packet);
    void SendCharacterListRequest();
    void HandleCharacterListResponse(Packet* packet);
    void SendCharacterSelection();
    void HandleMinifigureListResponse(Packet* packet);
    void SendWorldReady();
    
    // Movement simulation
    void GenerateRandomMovement();
    void SendPositionUpdate();
    
    // Member variables
    uint32_t m_ID;
    std::string m_Username;
    std::string m_Password;
    SimUserState m_State;
    
    // Network
    RakPeerInterface* m_AuthPeer;
    RakPeerInterface* m_WorldPeer;
    SystemAddress m_AuthServerAddr;
    SystemAddress m_WorldServerAddr;
    
    // Session data
    std::string m_SessionKey;
    LWOOBJID m_CharacterID;
    uint32_t m_WorldInstanceID;
    uint32_t m_ZoneID;
    
    // Position and movement
    NiPoint3 m_Position;
    NiQuaternion m_Rotation;
    NiPoint3 m_Velocity;
    NiPoint3 m_TargetPosition;
    bool m_IsMoving;
    
    // Timing
    std::chrono::steady_clock::time_point m_LastUpdate;
    std::chrono::steady_clock::time_point m_LastHeartbeat;
    std::chrono::steady_clock::time_point m_LastRandomAction;
    
    // Statistics
    uint64_t m_PacketsSent;
    uint64_t m_PacketsReceived;
    uint64_t m_BytesSent;
    uint64_t m_BytesReceived;
    
    // Error handling
    bool m_HasError;
    std::string m_LastError;
    
    // Random number generation
    mutable std::mt19937 m_RandomEngine;
    mutable std::uniform_real_distribution<float> m_RandomFloat;
};
