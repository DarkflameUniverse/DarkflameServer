#include "SimUser.h"
#include "RakPeerInterface.h"
#include "RakNetworkFactory.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"
#include "Logger.h"
#include "dNetCommon.h"
#include "Game.h"

#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

SimUser::SimUser(uint32_t id, const std::string& username, const std::string& password)
    : m_ID(id)
    , m_Username(username)
    , m_Password(password)
    , m_State(SimUserState::DISCONNECTED)
    , m_AuthPeer(nullptr)
    , m_WorldPeer(nullptr)
    , m_CharacterID(LWOOBJID_EMPTY)
    , m_WorldInstanceID(0)
    , m_ZoneID(1000) // Default to Venture Explorer
    , m_Position(NiPoint3::ZERO)
    , m_Rotation(NiQuaternion::IDENTITY)
    , m_Velocity(NiPoint3::ZERO)
    , m_TargetPosition(NiPoint3::ZERO)
    , m_IsMoving(false)
    , m_PacketsSent(0)
    , m_PacketsReceived(0)
    , m_BytesSent(0)
    , m_BytesReceived(0)
    , m_HasError(false)
    , m_RandomEngine(std::random_device{}())
    , m_RandomFloat(0.0f, 1.0f)
{
    m_LastUpdate = std::chrono::steady_clock::now();
    m_LastHeartbeat = std::chrono::steady_clock::now();
    m_LastRandomAction = std::chrono::steady_clock::now();
}

SimUser::~SimUser() {
    Disconnect();
}

bool SimUser::ConnectToAuth(const std::string& authIP, uint16_t authPort) {
    if (m_State != SimUserState::DISCONNECTED) {
        SetError("Cannot connect to auth server: already connected or in error state");
        return false;
    }

    m_AuthPeer = RakNetworkFactory::GetRakPeerInterface();
    if (!m_AuthPeer) {
        SetError("Failed to create RakPeer interface for auth connection");
        return false;
    }

    SocketDescriptor socketDescriptor(0, nullptr);
    if (m_AuthPeer->Startup(1, 30, &socketDescriptor, 1) != RAKNET_STARTED) {
        SetError("Failed to startup auth peer");
        RakNetworkFactory::DestroyRakPeerInterface(m_AuthPeer);
        m_AuthPeer = nullptr;
        return false;
    }

    if (m_AuthPeer->Connect(authIP.c_str(), authPort, nullptr, 0) != CONNECTION_ATTEMPT_STARTED) {
        SetError("Failed to start connection attempt to auth server");
        m_AuthPeer->Shutdown(0);
        RakNetworkFactory::DestroyRakPeerInterface(m_AuthPeer);
        m_AuthPeer = nullptr;
        return false;
    }

    m_AuthServerAddr = SystemAddress(authIP.c_str(), authPort);
    SetState(SimUserState::CONNECTING_TO_AUTH);
    LogMessage("Connecting to auth server at " + authIP + ":" + std::to_string(authPort));
    
    return true;
}

bool SimUser::ConnectToWorld(const std::string& worldIP, uint16_t worldPort) {
    if (m_State != SimUserState::AUTHENTICATING && m_State != SimUserState::CONNECTING_TO_WORLD) {
        SetError("Cannot connect to world server: not in correct state");
        return false;
    }

    m_WorldPeer = RakNetworkFactory::GetRakPeerInterface();
    if (!m_WorldPeer) {
        SetError("Failed to create RakPeer interface for world connection");
        return false;
    }

    SocketDescriptor socketDescriptor(0, nullptr);
    if (m_WorldPeer->Startup(1, 30, &socketDescriptor, 1) != RAKNET_STARTED) {
        SetError("Failed to startup world peer");
        RakNetworkFactory::DestroyRakPeerInterface(m_WorldPeer);
        m_WorldPeer = nullptr;
        return false;
    }

    if (m_WorldPeer->Connect(worldIP.c_str(), worldPort, nullptr, 0) != CONNECTION_ATTEMPT_STARTED) {
        SetError("Failed to start connection attempt to world server");
        m_WorldPeer->Shutdown(0);
        RakNetworkFactory::DestroyRakPeerInterface(m_WorldPeer);
        m_WorldPeer = nullptr;
        return false;
    }

    m_WorldServerAddr = SystemAddress(worldIP.c_str(), worldPort);
    SetState(SimUserState::CONNECTING_TO_WORLD);
    LogMessage("Connecting to world server at " + worldIP + ":" + std::to_string(worldPort));
    
    return true;
}

void SimUser::Disconnect() {
    if (m_AuthPeer) {
        m_AuthPeer->Shutdown(100);
        RakNetworkFactory::DestroyRakPeerInterface(m_AuthPeer);
        m_AuthPeer = nullptr;
    }
    
    if (m_WorldPeer) {
        m_WorldPeer->Shutdown(100);
        RakNetworkFactory::DestroyRakPeerInterface(m_WorldPeer);
        m_WorldPeer = nullptr;
    }
    
    SetState(SimUserState::DISCONNECTED);
}

void SimUser::ProcessIncomingPackets() {
    // Process auth server packets
    if (m_AuthPeer) {
        Packet* packet = nullptr;
        while ((packet = m_AuthPeer->Receive()) != nullptr) {
            m_PacketsReceived++;
            m_BytesReceived += packet->length;
            
            HandleAuthPacket(packet);
            m_AuthPeer->DeallocatePacket(packet);
        }
    }
    
    // Process world server packets
    if (m_WorldPeer) {
        Packet* packet = nullptr;
        while ((packet = m_WorldPeer->Receive()) != nullptr) {
            m_PacketsReceived++;
            m_BytesReceived += packet->length;
            
            HandleWorldPacket(packet);
            m_WorldPeer->DeallocatePacket(packet);
        }
    }
}

void SimUser::SendHeartbeat() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastHeartbeat);
    
    if (timeSinceLastHeartbeat.count() >= 5000) { // Send heartbeat every 5 seconds
        // Send to appropriate server based on current state
        RakNet::BitStream heartbeat;
        heartbeat.Write(static_cast<MessageID>(ID_USER_PACKET_ENUM + 1)); // Simple heartbeat
        
        if (m_State == SimUserState::IN_WORLD && m_WorldPeer) {
            m_WorldPeer->Send(&heartbeat, HIGH_PRIORITY, RELIABLE, 0, m_WorldServerAddr, false);
            m_PacketsSent++;
            m_BytesSent += heartbeat.GetNumberOfBytesUsed();
        }
        
        m_LastHeartbeat = now;
    }
}

void SimUser::Update(float deltaTime) {
    ProcessIncomingPackets();
    SendHeartbeat();
    
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastAction = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastRandomAction);
    
    // Perform random actions based on current state
    if (m_State == SimUserState::IN_WORLD && timeSinceLastAction.count() >= 5000) {
        SimulateRandomAction();
        m_LastRandomAction = now;
    }
    
    // Update movement simulation
    if (m_IsMoving) {
        SimulateMovement();
    }
}

void SimUser::SimulateMovement() {
    // Simple movement simulation - move towards target position
    const float moveSpeed = 5.0f; // units per second
    const float deltaTime = 0.016f; // Assuming 60 FPS
    
    NiPoint3 direction = m_TargetPosition - m_Position;
    float distance = direction.Length();
    
    if (distance > 0.1f) {
        direction = direction / distance; // Normalize
        NiPoint3 movement = direction * moveSpeed * deltaTime;
        
        if (movement.Length() >= distance) {
            m_Position = m_TargetPosition;
            m_IsMoving = false;
        } else {
            m_Position = m_Position + movement;
        }
        
        SendPositionUpdate();
    } else {
        m_IsMoving = false;
    }
}

void SimUser::SimulateRandomAction() {
    if (m_State != SimUserState::IN_WORLD) return;
    
    float action = m_RandomFloat(m_RandomEngine);
    
    if (action < 0.6f) {
        // Generate random movement
        GenerateRandomMovement();
    } else if (action < 0.8f) {
        // Send a chat message (if enabled)
        SendChatMessage("Hello from simulated client " + std::to_string(m_ID));
    } else {
        // Just idle
        LogMessage("Idling...");
    }
}

void SimUser::SendChatMessage(const std::string& message) {
    if (m_State != SimUserState::IN_WORLD || !m_WorldPeer) return;
    
    // Create a simple chat message
    RakNet::BitStream chatBitStream;
    chatBitStream.Write(static_cast<MessageID>(ID_USER_PACKET_ENUM + 2));
    chatBitStream.Write(static_cast<uint8_t>(0)); // General chat channel
    chatBitStream.Write(static_cast<uint16_t>(0)); // Unknown
    
    // Convert message to UTF-16
    std::u16string u16message;
    for (char c : message) {
        u16message.push_back(static_cast<char16_t>(c));
    }
    
    chatBitStream.Write(static_cast<uint32_t>(u16message.length()));
    for (char16_t c : u16message) {
        chatBitStream.Write(c);
    }
    
    m_WorldPeer->Send(&chatBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_WorldServerAddr, false);
    m_PacketsSent++;
    m_BytesSent += chatBitStream.GetNumberOfBytesUsed();
    
    LogMessage("Sent chat message: " + message);
}

void SimUser::GenerateRandomMovement() {
    // Generate a random target position within a reasonable area
    float randomX = (m_RandomFloat(m_RandomEngine) - 0.5f) * 20.0f; // -10 to 10
    float randomZ = (m_RandomFloat(m_RandomEngine) - 0.5f) * 20.0f; // -10 to 10
    
    m_TargetPosition = m_Position + NiPoint3(randomX, 0, randomZ);
    m_IsMoving = true;
    
    LogMessage("Started movement to (" + std::to_string(m_TargetPosition.x) + ", " + 
               std::to_string(m_TargetPosition.y) + ", " + std::to_string(m_TargetPosition.z) + ")");
}

void SimUser::SendPositionUpdate() {
    if (m_State != SimUserState::IN_WORLD || !m_WorldPeer) return;
    
    // Create position update packet
    RakNet::BitStream positionBitStream;
    positionBitStream.Write(static_cast<MessageID>(ID_USER_PACKET_ENUM + 3)); // Position update
    positionBitStream.Write(m_Position.x);
    positionBitStream.Write(m_Position.y); 
    positionBitStream.Write(m_Position.z);
    positionBitStream.Write(m_Rotation.x);
    positionBitStream.Write(m_Rotation.y);
    positionBitStream.Write(m_Rotation.z);
    positionBitStream.Write(m_Rotation.w);
    
    m_WorldPeer->Send(&positionBitStream, HIGH_PRIORITY, UNRELIABLE_SEQUENCED, 0, m_WorldServerAddr, false);
    m_PacketsSent++;
    m_BytesSent += positionBitStream.GetNumberOfBytesUsed();
}

void SimUser::HandleAuthPacket(Packet* packet) {
    switch (packet->data[0]) {
        case ID_CONNECTION_REQUEST_ACCEPTED:
            LogMessage("Connected to auth server");
            SendLoginRequest();
            break;
            
        case ID_DISCONNECTION_NOTIFICATION:
        case ID_CONNECTION_LOST:
            LogMessage("Disconnected from auth server");
            SetError("Lost connection to auth server");
            break;
            
        default:
            // Handle auth-specific packets
            if (packet->data[0] >= ID_USER_PACKET_ENUM) {
                HandleLoginResponse(packet);
            }
            break;
    }
}

void SimUser::HandleWorldPacket(Packet* packet) {
    switch (packet->data[0]) {
        case ID_CONNECTION_REQUEST_ACCEPTED:
            LogMessage("Connected to world server");
            SendWorldLoginRequest();
            break;
            
        case ID_DISCONNECTION_NOTIFICATION:
        case ID_CONNECTION_LOST:
            LogMessage("Disconnected from world server");
            SetError("Lost connection to world server");
            break;
            
        default:
            // Handle world-specific packets
            if (packet->data[0] >= ID_USER_PACKET_ENUM) {
                // For simplicity, just log that we received a world packet
                LogMessage("Received world packet of type " + std::to_string(packet->data[0]));
            }
            break;
    }
}

void SimUser::SendLoginRequest() {
    if (!m_AuthPeer) return;
    
    SetState(SimUserState::AUTHENTICATING);
    
    // Create login request packet
    RakNet::BitStream loginBitStream;
    loginBitStream.Write(static_cast<MessageID>(ID_USER_PACKET_ENUM));
    loginBitStream.Write(static_cast<uint32_t>(m_Username.length()));
    loginBitStream.Write(m_Username.c_str(), m_Username.length());
    loginBitStream.Write(static_cast<uint32_t>(m_Password.length()));
    loginBitStream.Write(m_Password.c_str(), m_Password.length());
    
    m_AuthPeer->Send(&loginBitStream, HIGH_PRIORITY, RELIABLE, 0, m_AuthServerAddr, false);
    m_PacketsSent++;
    m_BytesSent += loginBitStream.GetNumberOfBytesUsed();
    
    LogMessage("Sent login request for user: " + m_Username);
}

void SimUser::HandleLoginResponse(Packet* packet) {
    // Simple login response handling
    LogMessage("Received login response");
    
    // For simulation purposes, assume successful login and move to world
    m_SessionKey = "sim_session_" + std::to_string(m_ID);
    
    // Simulate connecting to world server (hardcoded for now)
    ConnectToWorld("127.0.0.1", 2000);
}

void SimUser::SendWorldLoginRequest() {
    if (!m_WorldPeer) return;
    
    SetState(SimUserState::LOADING_WORLD);
    
    // Create world login request
    RakNet::BitStream worldLoginBitStream;
    worldLoginBitStream.Write(static_cast<MessageID>(ID_USER_PACKET_ENUM + 10));
    worldLoginBitStream.Write(static_cast<uint32_t>(m_SessionKey.length()));
    worldLoginBitStream.Write(m_SessionKey.c_str(), m_SessionKey.length());
    
    m_WorldPeer->Send(&worldLoginBitStream, HIGH_PRIORITY, RELIABLE, 0, m_WorldServerAddr, false);
    m_PacketsSent++;
    m_BytesSent += worldLoginBitStream.GetNumberOfBytesUsed();
    
    LogMessage("Sent world login request");
    
    // For simulation, immediately consider ourselves in world
    SetState(SimUserState::IN_WORLD);
    m_Position = NiPoint3(0, 0, 0); // Start at origin
    LogMessage("Entered world simulation mode");
}

void SimUser::SetError(const std::string& error) {
    m_HasError = true;
    m_LastError = error;
    m_State = SimUserState::ERROR_STATE;
    LogMessage("ERROR: " + error);
}

void SimUser::LogMessage(const std::string& message) {
    if (Game::logger) {
        LOG("[SimUser %u (%s)] %s", m_ID, m_Username.c_str(), message.c_str());
    } else {
        std::cout << "[SimUser " << m_ID << " (" << m_Username << ")] " << message << std::endl;
    }
}
