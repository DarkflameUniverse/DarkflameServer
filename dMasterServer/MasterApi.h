#pragma once 

#include <thread>

#include "httplib.h"
#include "nlohmann/json.hpp"

#include "dConfig.h"
#include "dCommonVars.h"
#include "InstanceManager.h"

class dMasterServerApi {
public:
    dMasterServerApi(dConfig* config, InstanceManager* instanceManager);
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

    httplib::Server* m_HttpServer = nullptr;
    // TODO: Implement a SSL server, this requires OpenSSL and I really didn't feel like shooting myself in the face
};