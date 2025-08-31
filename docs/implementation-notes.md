# DarkflameServer Implementation Notes

## Key Implementation Files by Server Type

### Master Server (`dMasterServer/`)
- **MasterServer.cpp**: Main server loop and packet handling
- **InstanceManager.cpp**: Manages zone instances and player distribution
- **PersistentIDManager.cpp**: Generates unique object IDs across servers
- **Start.cpp**: Utility functions for starting other server types

### Authentication Server (`dAuthServer/`)
- **AuthServer.cpp**: Handles login, account creation, session management
- **AuthPackets.cpp**: Authentication protocol implementation

### Chat Server (`dChatServer/`)
- **ChatServer.cpp**: Main chat server with social features
- **ChatPacketHandler.cpp**: Routes chat and social messages
- **PlayerContainer.cpp**: Manages online player registry
- **TeamContainer.cpp**: Handles team/group functionality

### World Server (`dWorldServer/`)
- **WorldServer.cpp**: Game world simulation and entity management
- **PerformanceManager.cpp**: Monitors server performance metrics

### Game Logic (`dGame/`)
- **CheatDetection.cpp**: Anti-cheat system with database logging
- **UserManager.cpp**: Player session and character management
- **EntityManager.cpp**: Game entity lifecycle and updates

### Networking Layer (`dNet/`)
- **dServer.cpp/h**: Base server class with master communication
- **MasterPackets.cpp/h**: Master server protocol implementation
- **AuthPackets.cpp/h**: Authentication protocol
- **ChatPackets.cpp/h**: Chat and social protocols
- **WorldPackets.cpp/h**: World server protocols

## Message Flow Implementation

### Packet Structure
```cpp
// All packets use this basic structure:
BitStreamUtils::WriteHeader(bitStream, ServiceType, MessageType);
// Followed by message-specific data
```

### Service Types (from `ServiceType.h`)
```cpp
enum class ServiceType : uint16_t {
    COMMON = 0,
    AUTH,       // Authentication Server
    CHAT,       // Chat Server  
    WORLD = 4,  // World Server
    CLIENT,     // Client messages
    MASTER,     // Master Server
    UNKNOWN
};
```

### Critical Communication Patterns

#### 1. Server-to-Master Registration
```cpp
// All servers connect to master on startup
void dServer::ConnectToMaster() {
    mMasterPeer->Connect(mMasterIP.c_str(), mMasterPort, 
                        mMasterPassword.c_str(), mMasterPassword.size());
}

// On connection, servers send their info
case ID_CONNECTION_REQUEST_ACCEPTED: {
    MasterPackets::SendServerInfo(this, packet);
    break;
}
```

#### 2. Zone Transfer Protocol
```cpp
// World Server requests transfer
MasterPackets::SendZoneTransferRequest(server, requestID, mythranShift, zoneID, cloneID);

// Master finds/creates instance and responds
void HandleZoneTransferRequest() {
    auto instance = Game::im->GetInstance(zoneID, false, zoneClone);
    // Send response with target server info
}
```

#### 3. Chat Message Routing
```cpp
// World Server forwards chat to Chat Server
void HandleChatMessage() {
    // Route through Chat Server for processing
    Game::chatServer->Send(bitStream, chatServerAddr, false);
}

// Chat Server routes to all relevant World Servers
void RouteMessage() {
    for (auto& worldServer : connectedWorldServers) {
        Send(message, worldServer.address, false);
    }
}
```

## Database Architecture

### Core Tables
- **accounts**: User account information
- **charinfo**: Character data and statistics  
- **friends**: Friend relationships
- **mail**: In-game mail system
- **properties**: Player property data
- **leaderboards**: Competition scores
- **servers**: Server configuration

### Connection Management
```cpp
// Shared database connection across all servers
Database::Connect(); // Uses connection pooling
auto result = Database::Get()->Query(sql);
```

## Scalability Design

### Load Distribution
- Master Server acts as load balancer
- Multiple World Servers can run different zones
- Instance Manager distributes players across zones
- Chat Server handles all social features centrally

### Fault Tolerance
- All servers maintain connections with Master Server
- Database connection monitoring and error handling
- Player state persisted in database
- Graceful degradation when servers unavailable

### Performance Optimizations
- Entity serialization only to relevant players (ghosting)
- Spatial partitioning for efficient updates
- Database query optimization with prepared statements
- Network packet compression and optimization

## Security Features

### Network Security
- RakNet encryption for client connections
- Server-to-server authentication via passwords
- Session key validation across servers
- Packet validation and sanitization

### Game Security
- Server-side validation of all game actions
- Cheat detection system with database logging (`CheatDetection.cpp`)
- Player cheat detection table tracks suspicious activities
- Rate limiting on client requests
- Database injection prevention

## Development Notes

### Build System
- CMake-based build with modular components
- Cross-platform support (Windows, Linux, macOS)
- Dependency management through git submodules
- Automated testing framework

### Configuration
- INI-based configuration files per server type
- Environment variable support
- Runtime configuration updates
- Database-driven server settings

### Debugging Features
- Comprehensive logging system
- Performance metrics collection
- Packet capture and analysis tools
- Real-time server monitoring

This implementation successfully recreates the LEGO Universe MMO experience through a robust, scalable architecture that can support thousands of concurrent players across multiple game zones while maintaining the original game's functionality and performance characteristics.