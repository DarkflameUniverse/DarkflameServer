#pragma once 

#include <thread>

#include "httplib.h"
#include "nlohmann/json.hpp"

#include "dConfig.h"
#include "dCommonVars.h"
#include "InstanceManager.h"
#include "MasterPackets.h"
#include "ChatPackets.h"
#include "WorldPackets.h"
#include "AMFFormat.h"
#include "dCommonVars.h"
#include "dMessageIdentifiers.h"
#include "PacketUtils.h"
#include "GameMessages.h"
#include "dServer.h"

#define API_VERSION "v1"

using namespace nlohmann::detail;

// JSON Helpers

inline bool CheckJSONTypes(nlohmann::json json, std::unordered_map<std::string, value_t> keys) {
    for (const auto& kv : keys) {
        if (json.find(kv.first) == json.end() && json[kv.first].size() == 0 && json[kv.first].type() == kv.second) {
            return false;
        }
    }

    return true;
}

class dMasterServerApi {
public:
    dMasterServerApi(dConfig* config, InstanceManager* instanceManager, dServer* server);
    ~dMasterServerApi();

    /**
     * @brief Attempts to listen on the configured port and address.
     * @return whether it was able to listen
     */
    bool Listen();
private:
    /**
     * @brief thread to do listening as the http server is blocking
     */
    void ListenerThread();

    /**
     * @brief Create a the routes used by the http server
     */
    void CreateRoutes();

    bool m_ReadyToListen = false;

    uint32_t m_Port;
    std::string m_Host;

    InstanceManager* m_InstanceManager;

    dServer* m_Server;

    httplib::Server* m_HttpServer = nullptr;
    // TODO: Implement a SSL server, this requires OpenSSL and I really didn't feel like shooting myself in the face
};