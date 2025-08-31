# DarkflameServer Architecture - ASCII Diagram

```
                                DARKFLAME SERVER ARCHITECTURE
                                ===============================

    ┌─────────────────────────────────────────────────────────────────────────────────────────┐
    │                                   CLIENT LAYER                                          │
    │  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐              │
    │  │LEGO Universe│    │LEGO Universe│    │LEGO Universe│    │LEGO Universe│              │
    │  │   Client    │    │   Client    │    │   Client    │    │   Client    │              │
    │  │  (Player)   │    │  (Player)   │    │  (Player)   │    │  (Player)   │              │
    │  └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘              │
    │         │                   │                   │                   │                   │
    └─────────┼───────────────────┼───────────────────┼───────────────────┼───────────────────┘
              │                   │                   │                   │
              │                   │                   │                   │
    ┌─────────┼───────────────────┼───────────────────┼───────────────────┼───────────────────┐
    │         │        SERVER COMMUNICATION LAYER     │                   │                   │
    │         │                   │                   │                   │                   │
    │         ▼                   ▼                   ▼                   ▼                   │
    │  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐              │
    │  │AUTH SERVER │    │CHAT SERVER │    │WORLD SERVER │    │WORLD SERVER │              │
    │  │             │    │             │    │             │    │             │              │
    │  │Port: 1001   │    │Port: 1501   │    │Port: Dynamic│    │Port: Dynamic│              │
    │  │             │    │             │    │             │    │             │              │
    │  │• Login      │    │• Chat Msgs  │    │• Game Logic │    │• Game Logic │              │
    │  │• Accounts   │    │• Teams      │    │• Entities   │    │• Entities   │              │
    │  │• Sessions   │    │• Guilds     │    │• Physics    │    │• Physics    │              │
    │  │• Keys       │    │• Friends    │    │• Zone Mgmt  │    │• Zone Mgmt  │              │
    │  └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘              │
    │         │                   │                   │                   │                   │
    │         └───────────────────┼───────────────────┼───────────────────┘                   │
    │                             │                   │                                       │
    │                             │                   │                                       │
    │         ┌───────────────────┼───────────────────┼───────────────────┐                   │
    │         │                   │                   │                   │                   │
    │         ▼                   ▼                   ▼                   ▼                   │
    │    ┌─────────────────────────────────────────────────────────────────────────────┐      │
    │    │                      MASTER SERVER (Port: 1500)                           │      │
    │    │                                                                            │      │
    │    │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐         │      │
    │    │  │ Instance Manager │  │Persistent ID Mgr │  │  Session Manager │         │      │
    │    │  │                  │  │                  │  │                  │         │      │
    │    │  │• Zone Instances  │  │• Object ID Gen   │  │• Player Sessions │         │      │
    │    │  │• Load Balance    │  │• Unique IDs      │  │• Server Registry │         │      │
    │    │  │• Player Transfer │  │• Cross-Server    │  │• Coordination    │         │      │
    │    │  └──────────────────┘  └──────────────────┘  └──────────────────┘         │      │
    │    └─────────────────────────────────────────────────────────────────────────────┘      │
    │                                        │                                                │
    └────────────────────────────────────────┼────────────────────────────────────────────────┘
                                             │
                                             ▼
                       ┌─────────────────────────────────────────┐
                       │              DATABASE LAYER             │
                       │                                         │
                       │        ┌─────────────────────┐          │
                       │        │    MySQL Database   │          │
                       │        │                     │          │
                       │        │ • Player Data       │          │
                       │        │ • Characters        │          │
                       │        │ • Friends/Mail      │          │
                       │        │ • Properties        │          │
                       │        │ • Leaderboards      │          │
                       │        │ • Configuration     │          │
                       │        └─────────────────────┘          │
                       └─────────────────────────────────────────┘


                              MESSAGE FLOW OVERVIEW
                              ====================

    Auth Messages                Chat Messages                World Messages
    =============                =============                ==============
    
    LOGIN_REQUEST               GENERAL_CHAT_MESSAGE         CHARACTER_LIST_REQUEST
    CREATE_ACCOUNT              PRIVATE_CHAT_MESSAGE         GAME_MSG (1000+ types)
    SESSION_KEY                 TEAM_INVITE                  LEVEL_LOAD_COMPLETE
    AUTH_RESPONSE               ADD_FRIEND                   POSITION_UPDATE
                               GUILD_OPERATIONS              ZONE_TRANSFER_REQUEST

    Master Coordination Messages
    ============================
    
    SERVER_INFO                 REQUEST_PERSISTENT_ID         WORLD_READY
    PLAYER_ADDED/REMOVED        REQUEST_ZONE_TRANSFER         SHUTDOWN
    SET_SESSION_KEY             AFFIRM_TRANSFER              NEW_SESSION_ALERT


                              COMMUNICATION PATTERNS
                              ======================

    1. Client Authentication Flow:
       Client → Auth Server → Master Server → World Server → Chat Server

    2. Zone Transfer Flow:
       World Server → Master Server → Target World Server → Client

    3. Chat Message Flow:
       Client → World Server → Chat Server → Target World Server → Target Client

    4. Entity Updates Flow:
       World Server → EntityManager → ReplicaManager → All Clients in Zone

    5. Master Coordination:
       All Servers ↔ Master Server (Heartbeat, Registration, Coordination)


                              NETWORK SPECIFICATIONS
                              ======================

    Protocol: RakNet (UDP-based)
    Encryption: Enabled for client connections
    Packet Structure: [RakNet Header][Message Type][Service Type][Message Data]
    
    Service Type Codes:
    - MASTER (5): Server coordination
    - AUTH (1): Authentication
    - CHAT (2): Social features  
    - WORLD (4): Game simulation
    - CLIENT (4): Client-specific

    Key Features:
    - Distributed architecture for scalability
    - Fault tolerance through master coordination
    - Real-time entity replication
    - Cross-server player transfers
    - Persistent world state
```