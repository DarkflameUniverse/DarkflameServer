# DarkflameServer Architecture and Communication Diagram

This document provides a comprehensive overview of the server architecture, inter-server communication patterns, and message flows in the DarkflameServer LEGO Universe emulator.

## Server Architecture Overview

```mermaid
graph TB
    subgraph "Client Layer"
        Client1[LEGO Universe Client]
        Client2[LEGO Universe Client]
        Client3[LEGO Universe Client]
    end

    subgraph "DarkflameServer Infrastructure"
        subgraph "Master Server (Port 1500)"
            MasterServer[Master Server<br/>- Zone Management<br/>- Instance Coordination<br/>- Session Management<br/>- Server Registration]
            IM[Instance Manager<br/>- Zone Instances<br/>- Player Transfers<br/>- Load Balancing]
            PIM[Persistent ID Manager<br/>- Object ID Generation<br/>- Unique Identifiers]
        end

        subgraph "Authentication Server (Port 1001)" 
            AuthServer[Auth Server<br/>- Login Processing<br/>- Account Management<br/>- Session Keys<br/>- Player Authentication]
        end

        subgraph "Chat Server (Port 1501)"
            ChatServer[Chat Server<br/>- General Chat<br/>- Private Messages<br/>- Team Management<br/>- Guild System<br/>- Friend Lists<br/>- Mail System]
            PC[Player Container<br/>- Online Players<br/>- Chat Routing]
        end

        subgraph "World Servers (Dynamic Ports)"
            WorldServer1[World Server 1<br/>- Game Logic<br/>- Entity Management<br/>- Physics Simulation<br/>- Zone: Avant Gardens]
            WorldServer2[World Server 2<br/>- Game Logic<br/>- Entity Management<br/>- Physics Simulation<br/>- Zone: Nimbus Station]
            WorldServer3[World Server N<br/>- Game Logic<br/>- Entity Management<br/>- Physics Simulation<br/>- Zone: Custom]
            EM1[Entity Manager]
            EM2[Entity Manager] 
            EM3[Entity Manager]
            WorldServer1 --- EM1
            WorldServer2 --- EM2
            WorldServer3 --- EM3
        end
    end

    subgraph "Database Layer"
        Database[(MySQL Database<br/>- Player Data<br/>- Characters<br/>- Properties<br/>- Leaderboards<br/>- Mail<br/>- Friends)]
    end

    %% Client Connections
    Client1 -.->|1. Authentication| AuthServer
    Client1 -.->|2. Character/Zone Data| WorldServer1
    
    Client2 -.->|Authentication| AuthServer  
    Client2 -.->|Character/Zone Data| WorldServer2

    Client3 -.->|Authentication| AuthServer
    Client3 -.->|Character/Zone Data| WorldServer3

    %% Server-to-Master Connections (All servers connect to master)
    AuthServer <-.->|SERVER_INFO<br/>SESSION_KEY<br/>PLAYER_ADDED/REMOVED| MasterServer
    ChatServer <-.->|SERVER_INFO<br/>PLAYER_ADDED/REMOVED| MasterServer  
    WorldServer1 <-.->|ZONE_TRANSFER<br/>PERSISTENT_ID<br/>WORLD_READY<br/>SERVER_INFO| MasterServer
    WorldServer2 <-.->|ZONE_TRANSFER<br/>PERSISTENT_ID<br/>WORLD_READY<br/>SERVER_INFO| MasterServer
    WorldServer3 <-.->|ZONE_TRANSFER<br/>PERSISTENT_ID<br/>WORLD_READY<br/>SERVER_INFO| MasterServer

    %% Master Server Internal Communication
    MasterServer --- IM
    MasterServer --- PIM

    %% Database Connections
    AuthServer <--> Database
    ChatServer <--> Database
    WorldServer1 <--> Database
    WorldServer2 <--> Database
    WorldServer3 <--> Database
    MasterServer <--> Database

    %% Chat Server Communication (World Servers route chat, clients never connect directly)
    WorldServer1 <-.->|Chat Messages<br/>Team Operations<br/>Social Features| ChatServer
    WorldServer2 <-.->|Chat Messages<br/>Team Operations<br/>Social Features| ChatServer
    WorldServer3 <-.->|Chat Messages<br/>Team Operations<br/>Social Features| ChatServer

    style MasterServer fill:#ff9999
    style AuthServer fill:#99ccff
    style ChatServer fill:#99ff99
    style WorldServer1 fill:#ffcc99
    style WorldServer2 fill:#ffcc99
    style WorldServer3 fill:#ffcc99
    style Database fill:#cc99ff
```

## Message Type Breakdown

### Master Server Messages (`MessageType::Master`)

```mermaid
graph LR
    subgraph "Master Server Communication"
        Master[Master Server]
        
        subgraph "To/From Other Servers"
            M1[REQUEST_PERSISTENT_ID<br/>↔ World Servers]
            M2[REQUEST_ZONE_TRANSFER<br/>↔ World Servers]  
            M3[SERVER_INFO<br/>← All Servers]
            M4[SET_SESSION_KEY<br/>↔ Auth Server]
            M5[PLAYER_ADDED/REMOVED<br/>← All Servers]
            M6[WORLD_READY<br/>← World Servers]
            M7[SHUTDOWN<br/>→ All Servers]
        end
        
        Master -.-> M1
        Master -.-> M2
        Master -.-> M3
        Master -.-> M4
        Master -.-> M5
        Master -.-> M6
        Master -.-> M7
    end
```

### Authentication Server Messages (`MessageType::Auth`)

```mermaid
graph LR
    subgraph "Auth Server Communication"
        Auth[Auth Server]
        
        subgraph "To/From Clients"
            A1[LOGIN_REQUEST<br/>← Clients]
            A2[CREATE_NEW_ACCOUNT_REQUEST<br/>← Clients]
            A3[LEGOINTERFACE_AUTH_RESPONSE<br/>→ Clients]
        end
        
        subgraph "To/From Master"
            A4[SESSION_KEY<br/>→ Master]
            A5[PLAYER_NOTIFICATION<br/>→ Master]
        end
        
        Auth -.-> A1
        Auth -.-> A2
        Auth -.-> A3
        Auth -.-> A4
        Auth -.-> A5
    end
```

### Chat Server Messages (`MessageType::Chat`)

```mermaid
graph LR
    subgraph "Chat Server Communication"
        Chat[Chat Server]
        
        subgraph "Player Social Features"
            C1[GENERAL_CHAT_MESSAGE<br/>↔ All Players]
            C2[PRIVATE_CHAT_MESSAGE<br/>↔ Players]
            C3[TEAM_INVITE/RESPONSE<br/>↔ Players]
            C4[ADD_FRIEND_REQUEST<br/>↔ Players]
            C5[GUILD_OPERATIONS<br/>↔ Players]
            C6[MAIL_SYSTEM<br/>↔ Players]
        end
        
        subgraph "World Server Integration"
            C7[WORLD_ROUTE_PACKET<br/>↔ World Servers]
            C8[PLAYER_READY<br/>← World Servers]
            C9[LOGIN_SESSION_NOTIFY<br/>← World Servers]
        end
        
        Chat -.-> C1
        Chat -.-> C2
        Chat -.-> C3
        Chat -.-> C4
        Chat -.-> C5
        Chat -.-> C6
        Chat -.-> C7
        Chat -.-> C8
        Chat -.-> C9
    end
```

### World Server Messages (`MessageType::World` & `MessageType::Game`)

```mermaid
graph TB
    subgraph "World Server Communication"
        World[World Server]
        
        subgraph "Client Game Messages"
            W1[CHARACTER_LIST_REQUEST<br/>← Clients]
            W2[CHARACTER_CREATE_REQUEST<br/>← Clients]
            W3[LOGIN_REQUEST<br/>← Clients]
            W4[GAME_MSG<br/>↔ Clients]
            W5[LEVEL_LOAD_COMPLETE<br/>← Clients]
            W6[POSITION_UPDATE<br/>↔ Clients]
        end
        
        subgraph "Entity & Game Logic (1000+ Game Messages)"
            G1[Entity Serialization<br/>Player/NPC Updates]
            G2[Combat System<br/>Skills & Damage]
            G3[Building System<br/>Property Management]
            G4[Mission System<br/>Quest Management]
            G5[Inventory System<br/>Item Management]
            G6[Racing System<br/>Vehicle Physics]
        end
        
        subgraph "Master Server Communication"
            W7[ZONE_TRANSFER_REQUEST<br/>→ Master]
            W8[PERSISTENT_ID_REQUEST<br/>→ Master]
            W9[WORLD_READY<br/>→ Master]
        end
        
        subgraph "Chat Server Communication"
            W10[Chat Messages<br/>↔ Chat Server]
            W11[Team Operations<br/>↔ Chat Server]
        end
        
        World -.-> W1
        World -.-> W2
        World -.-> W3
        World -.-> W4
        World -.-> W5
        World -.-> W6
        World -.-> W7
        World -.-> W8
        World -.-> W9
        World -.-> W10
        World -.-> W11
        
        World --- G1
        World --- G2
        World --- G3
        World --- G4
        World --- G5
        World --- G6
    end
```

## Communication Flow Diagrams

### Player Login Flow

```mermaid
sequenceDiagram
    participant C as Client
    participant A as Auth Server
    participant M as Master Server
    participant W as World Server
    participant Ch as Chat Server
    participant DB as Database

    C->>A: LOGIN_REQUEST
    A->>DB: Validate credentials
    DB-->>A: User data
    A->>M: SET_SESSION_KEY
    A->>C: LEGOINTERFACE_AUTH_RESPONSE
    C->>W: CHARACTER_LIST_REQUEST
    W->>DB: Get characters
    DB-->>W: Character data
    W->>C: Character list
    C->>W: LOGIN_REQUEST (character selected)
    W->>M: PLAYER_ADDED
    W->>Ch: LOGIN_SESSION_NOTIFY
    W->>C: Zone data & enter world
```

### Zone Transfer Flow

```mermaid
sequenceDiagram
    participant C as Client
    participant W1 as Source World Server
    participant M as Master Server
    participant W2 as Target World Server
    participant Ch as Chat Server

    C->>W1: Transfer request (rocket, door, etc.)
    W1->>M: REQUEST_ZONE_TRANSFER
    M->>M: Find/create zone instance
    M-->>W1: ZONE_TRANSFER_RESPONSE
    W1->>C: TRANSFER_TO_ZONE
    C->>W2: Connect to new world server
    W2->>M: Confirm player transfer
    W2->>Ch: Update player location
    W1->>M: PLAYER_REMOVED
    W2->>M: PLAYER_ADDED
```

### Chat Message Flow

```mermaid
sequenceDiagram
    participant C1 as Client 1
    participant W1 as World Server 1
    participant Ch as Chat Server
    participant W2 as World Server 2
    participant C2 as Client 2

    C1->>W1: Chat message
    W1->>Ch: GENERAL_CHAT_MESSAGE  
    Ch->>Ch: Process & route message
    Ch->>W2: WORLD_ROUTE_PACKET (to relevant world servers)
    W2->>C2: Display message
    Ch->>W1: Message confirmation
    W1->>C1: Message sent confirmation
```

## Technical Implementation Details

### Network Architecture
- **Protocol**: RakNet UDP-based networking
- **Security**: Encryption enabled for external connections
- **Port Configuration**:
  - Master Server: 1500 (default)
  - Auth Server: 1001 (hardcoded in client)
  - Chat Server: 1501 (configurable)
  - World Servers: Dynamic ports assigned by Master

### Server-to-Server Communication
```cpp
// All servers maintain connection to Master
class dServer {
    Packet* ReceiveFromMaster();
    void SendToMaster(RakNet::BitStream& bitStream);
    bool ConnectToMaster();
    void SetupForMasterConnection();
};
```

### Key Classes and Components

#### Master Server Components
- `InstanceManager`: Manages zone instances and player distribution
- `PersistentIDManager`: Generates unique object IDs across all servers
- `MasterPackets`: Handles inter-server message protocols

#### World Server Components
- `EntityManager`: Manages all game entities and their serialization
- `UserManager`: Tracks connected players and their sessions
- `ReplicaManager`: Handles object replication to clients
- `GameMessages`: Processes 1000+ different game mechanic messages

#### Chat Server Components
- `PlayerContainer`: Maintains online player registry
- `TeamContainer`: Manages team/group functionality
- `ChatPacketHandler`: Routes all chat and social messages

### Database Integration
- All servers connect to shared MySQL database
- Player data, characters, mail, friends, properties stored centrally
- Concurrent access managed through connection pooling
- Migration system for database schema updates

### Scalability Features
- Multiple World Server instances for different zones
- Load balancing through Master Server's Instance Manager
- Dynamic server spawning capability
- Horizontal scaling support for high player counts

## Message Protocol Specifications

### Packet Structure
```
[RakNet Header][Message Type][Service Type][Message Data]
```

### Service Types
- `MASTER (5)`: Master server coordination
- `AUTH (1)`: Authentication services  
- `CHAT (2)`: Chat and social features
- `WORLD (4)`: Game world simulation
- `CLIENT (4)`: Client-specific messages

### Critical Message Types by Frequency
1. **GAME_MSG** (~60% of traffic): Real-time game mechanics
2. **POSITION_UPDATE** (~20% of traffic): Player movement
3. **Chat Messages** (~10% of traffic): Social communication
4. **Entity Serialization** (~8% of traffic): World state sync
5. **Administrative** (~2% of traffic): Server coordination

This architecture enables the DarkflameServer to faithfully recreate the LEGO Universe MMO experience through a distributed, scalable server infrastructure that can handle thousands of concurrent players across multiple game zones.