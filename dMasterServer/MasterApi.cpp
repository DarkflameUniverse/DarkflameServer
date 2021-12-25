#include "MasterApi.h"

dMasterServerApi::dMasterServerApi(dConfig* config, InstanceManager* instanceManager, dServer* server) {
    m_HttpServer = new httplib::Server();

    m_Server = server;

    m_InstanceManager = instanceManager;

    if (config->HasKey("api_port") && config->HasKey("api_host_address")) {
        m_Port = std::stoi(config->GetValue("api_port"));
        m_Host = config->GetValue("api_host_address");
        this->CreateRoutes();
        m_ReadyToListen = true;
    }
}

void dMasterServerApi::CreateRoutes() {
    // Getters

    m_HttpServer->Get("/instances", [this](const httplib::Request& req, httplib::Response& res) {
        
        auto json = nlohmann::json();
        json["success"] = true;
        json["instances"] = nlohmann::json::array();
        
        for (auto* item : this->m_InstanceManager->GetInstances()) {
            json["instances"].push_back(item->GetJson());
        }
        
        res.set_content(json.dump(), "application/json");
    });

    // Setters
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