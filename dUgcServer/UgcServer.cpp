#include <httplib.h>

#include "Diagnostics.h"
#include "BinaryPathFinder.h"
#include "dLogger.h"
#include "Database.h"
#include "dConfig.h"
#include "dCommonVars.h"

#include "GZip.h"

#include "Game.h"
namespace Game {
	dLogger* logger = nullptr;
	dConfig* config = nullptr;

	httplib::Server* httpServer = nullptr;
}

dLogger* SetupLogger();
void SetupRoutes();

int main(int argc, char** argv) {
	Diagnostics::SetProcessName("UGC");
	Diagnostics::SetProcessFileName(argv[0]);
	Diagnostics::Initialize();

	Game::logger = SetupLogger();
	if (!Game::logger) return EXIT_FAILURE;

	Game::config = new dConfig((BinaryPathFinder::GetBinaryDir() / "ugcconfig.ini").string());
	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");

	Game::logger->Log("UgcServer", "Starting UGC server...");
	Game::logger->Log("UgcServer", "Version: %i.%i", PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR);
	Game::logger->Log("UgcServer", "Compiled on: %s", __TIMESTAMP__);


	try {
		Database::Connect(
			Game::config->GetValue("mysql_host"),
			Game::config->GetValue("mysql_database"),
			Game::config->GetValue("mysql_username"),
			Game::config->GetValue("mysql_password")
		);
	} catch (sql::SQLException& ex) {
		Game::logger->Log("UgcServer", "Got an error while connecting to the database: %s", ex.what());

		Database::Destroy("UgcServer");
		
		delete Game::logger;

		return EXIT_FAILURE;
	}

	Game::httpServer = new httplib::Server();

	SetupRoutes();

	Game::logger->Log("UgcServer", "Starting web server...");
	Game::httpServer->listen(Game::config->GetValue("external_ip").c_str(), 80);
}

void SetupRoutes() {
	// Alternate routes, exceptions etc
	Game::httpServer->set_error_handler([](const auto& req, auto& res) {
		auto fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
		char buf[BUFSIZ];
		snprintf(buf, sizeof(buf), fmt, res.status);
		res.set_content(buf, "text/html");
	});


	Game::httpServer->Get(R"(/3dservices/UGCC150/(\w+).lxfml.gz)", [](const httplib::Request& req, httplib::Response& res) {
		Game::logger->Log("UgcServer", "Receieved request for %s", req.matches[1].str().c_str());

		LWOOBJID blueprintId = std::stoull(req.matches[1].str());


	});
}

dLogger* SetupLogger() {
	std::string logPath = (BinaryPathFinder::GetBinaryDir() / ("logs/UgcServer_" + std::to_string(time(nullptr)) + ".log")).string();
	bool logToConsole, logDebugStatements = false;
	
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif

	return new dLogger(logPath, logToConsole, logDebugStatements);
}
