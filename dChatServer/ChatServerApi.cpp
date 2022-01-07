#include "ChatServerApi.h"

dChatServerApi::dChatServerApi(dConfig* config, dServer* server) {
    m_HttpServer = new httplib::Server();

    m_Server = server;

    if (config->HasKey("api_port")) {
        m_Port = std::stoi(config->GetValue("api_port"));
        m_Host = "127.0.0.1";
        this->CreateRoutes();
        m_ReadyToListen = true;
        Game::logger->Log("ChatApi", "API server ready to listen on port %i \n", m_Port);
    }
}

void dChatServerApi::CreateRoutes() {
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

    m_HttpServer->Get("/api/" API_VERSION "/players", [this](const httplib::Request& req, httplib::Response& res) {
        auto json = nlohmann::json();
        json["success"] = true;
        json["players"] = nlohmann::json::array();

        for (auto& item : PlayerContainer::Instance().GetAllPlayerData()) {
            json["players"].push_back(item.second->GetJson());
        }
        
        res.set_content(json.dump(), "application/json");
    });
}

bool dChatServerApi::Listen() {
    if (m_ReadyToListen) {
        std::thread listenerThread(&dChatServerApi::ListenerThread, this);
        listenerThread.detach();

        return true;
    }

    return false;
}

void dChatServerApi::ListenerThread() {
    m_HttpServer->listen(m_Host.c_str(), m_Port);
}

dChatServerApi::~dChatServerApi() {
    if (m_HttpServer->is_valid()) {
        m_HttpServer->stop();
    }

    delete m_HttpServer; // destroy out http server
}