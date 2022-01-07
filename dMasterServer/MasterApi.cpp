#include "MasterApi.h"

dMasterServerApi::dMasterServerApi(dConfig* config, InstanceManager* instanceManager, dServer* server) {
    m_HttpServer = new httplib::Server();

    m_Server = server;

    m_InstanceManager = instanceManager;

    if (config->HasKey("api_port")) {
        m_Port = std::stoi(config->GetValue("api_port"));
        m_Host = "127.0.0.1";
        this->CreateRoutes();
        m_ReadyToListen = true;
        Game::logger->Log("MasterApi", "API server ready to listen on port %i \n", m_Port);
    }
}

void dMasterServerApi::CreateRoutes() {
    // Documentation endpoints
    m_HttpServer->Get("/api/version", [this](const httplib::Request& req, httplib::Response& res) {
        auto json = nlohmann::json();
        json["version"] = API_VERSION;
#ifdef __TIMESTAMP__
        json["compile_time"] = __TIMESTAMP__;
#endif

        res.set_content(json.dump(), "application/json");
    });

    // GET Endpoints

    m_HttpServer->Get("/api/" API_VERSION "/instances", [this](const httplib::Request& req, httplib::Response& res) {
        
        auto json = nlohmann::json();
        json["success"] = true;
        json["instances"] = nlohmann::json::array();
        
        for (auto* item : this->m_InstanceManager->GetInstances()) {
            json["instances"].push_back(item->GetJson());
        }
        
        res.set_content(json.dump(), "application/json");
    });

    m_HttpServer->Get("/api/" API_VERSION "/shutdown_status", [this](const httplib::Request& req, httplib::Response& res) {
        // get status of shutdown instances

        auto responseJson = nlohmann::json();

        responseJson["success"] = false;

        if (!req.has_param("instance")) {
            responseJson["error"] = "Missing required fields";
            res.set_content(responseJson.dump(), "application/json");
            return;
        }

        if (req.get_param_value("instance") == "0.0.0.0") {
            responseJson["success"] = true;
            responseJson["result"] = nlohmann::json();
            for (auto* item : m_InstanceManager->GetInstances()) {
                if (item->GetShutdownRequested() && !item->GetShutdownComplete()) {
                    responseJson["result"][item->GetSysAddr().ToString(true)] = "Shutdown requested, waiting for response";
                }
            }

            for (auto item : m_InstanceManager->GetShutdownInstances()) {
                responseJson["result"][item.ToString(true)] = "Shutdown complete";
            }

            if (responseJson["result"].size() == 0) {
                responseJson["result"] = "No shutdowns requested";
            }

            res.set_content(responseJson.dump(), "application/json");
            return;
        } else {
            SystemAddress sysAddr;
            sysAddr.SetBinaryAddress(req.get_param_value("instance").c_str());
            auto* instance = this->m_InstanceManager->GetInstanceBySysAddr(sysAddr);
            auto possibleSystemAddress = UNASSIGNED_SYSTEM_ADDRESS;
            for (const auto item : m_InstanceManager->GetShutdownInstances()) {
                if (item == sysAddr) {
                    possibleSystemAddress = item;
                }
            }
            
            if (!instance && possibleSystemAddress == UNASSIGNED_SYSTEM_ADDRESS) {
                responseJson["error"] = "Failed to find instance";
                res.set_content(responseJson.dump(), "application/json");
                return;
            }

            responseJson["success"] = true;
            responseJson["result"] = nlohmann::json();

            if (instance) {
                if (instance->GetShutdownRequested() && !instance->GetShutdownComplete()) {
                    responseJson["result"][instance->GetSysAddr().ToString(true)] = "Shutdown requested, waiting for response";
                } else {
                    responseJson["error"] = "Server not requested for shutdown";
                    responseJson["success"] = false;
                    responseJson.erase("result");
                    res.set_content(responseJson.dump(), "application/json");
                    return;
                }
            } else {
                responseJson["result"][possibleSystemAddress.ToString(true)] = "Shutdown complete";
            }
            res.set_content(responseJson.dump(), "application/json");
        }
    });

    // POST Endpoints

    m_HttpServer->Post("/api/" API_VERSION "/broadcast", [this](const httplib::Request& req, httplib::Response& res) {
        auto responseJson = nlohmann::json();

        responseJson["success"] = false;

        if (!req.has_param("body") || !req.has_param("title") || !req.has_param("instance")) {
            responseJson["error"] = "Missing required fields";
            res.set_content(responseJson.dump(), "application/json");
            return;
        }
        
        std::vector<Instance*> instancesToBroadcastTo;

        CBITSTREAM;

        PacketUtils::WriteHeader(bitStream, MASTER, MSG_MASTER_BROADCAST);

        bitStream.Write(req.get_param_value("title"));
        bitStream.Write(req.get_param_value("body"));

        if (req.has_param("userAddr")) {
            bitStream.Write1();
            bitStream.Write(req.get_param_value("userAddr"));
        } else {
            bitStream.Write0();
        }

        if (req.get_param_value("instance") == "0.0.0.0") {
            for (const auto* item : m_InstanceManager->GetInstances()) {
                auto sysAddr = item->GetSysAddr();
                SEND_PACKET;
                Game::logger->Log("MasterServer", "Sending broadcast (%s, %s) to %i \n", req.get_param_value("title").c_str(), req.get_param_value("body").c_str(), item->GetMapID());
            }

            responseJson["success"] = true;
            responseJson["result"] = "Broadcasted to " + std::to_string(m_InstanceManager->GetInstances().size()) + " instances.";
            res.set_content(responseJson.dump(), "application/json");
            return;
        } else {
            SystemAddress sysAddr;
            sysAddr.SetBinaryAddress(req.get_param_value("instance").c_str());
            auto* instance = this->m_InstanceManager->GetInstanceBySysAddr(sysAddr);
            if (!instance) {
                responseJson["error"] = "Failed to find instance";
                res.set_content(responseJson.dump(), "application/json");
                return;
            }
            SEND_PACKET;
            responseJson["success"] = true;
            responseJson["result"] = std::string("Broadcasted to ") + sysAddr.ToString(true);
        }
    });

    m_HttpServer->Post("/api/" API_VERSION "/shutdown", [this](const httplib::Request& req, httplib::Response& res) {
        // shutdown instances or all of the server

        auto responseJson = nlohmann::json();

        responseJson["success"] = false;

        if (!req.has_param("instance")) {
            responseJson["error"] = "Missing required fields";
            res.set_content(responseJson.dump(), "application/json");
            return;
        }

        if (req.get_param_value("instance") == "0.0.0.0") {
            for (auto* item : m_InstanceManager->GetInstances()) {
                auto sysAddr = item->GetSysAddr();
                item->Shutdown();
            }

            responseJson["success"] = true;
            responseJson["result"] = "Sent shutdown request to " + std::to_string(m_InstanceManager->GetInstances().size()) + " instances.";
            res.set_content(responseJson.dump(), "application/json");
            return;
        } else {
            SystemAddress sysAddr;
            sysAddr.SetBinaryAddress(req.get_param_value("instance").c_str());
            auto* instance = this->m_InstanceManager->GetInstanceBySysAddr(sysAddr);
            if (!instance) {
                responseJson["error"] = "Failed to find instance";
                res.set_content(responseJson.dump(), "application/json");
                return;
            }

            instance->Shutdown();

            responseJson["success"] = true;
            responseJson["result"] = std::string("Sent shutdown request to ") + sysAddr.ToString(true);
        }
    });
}

bool dMasterServerApi::Listen() {
    if (m_ReadyToListen) {
        std::thread listenerThread(&dMasterServerApi::ListenerThread, this);
        listenerThread.detach();

        return true;
    }

    return false;
}

void dMasterServerApi::ListenerThread() {
    m_HttpServer->listen(m_Host.c_str(), m_Port);
}

dMasterServerApi::~dMasterServerApi() {
    if (m_HttpServer->is_valid()) {
        m_HttpServer->stop();
    }

    delete m_HttpServer; // destroy out http server
}