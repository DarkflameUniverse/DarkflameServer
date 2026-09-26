#include "Server.h"

#include "BinaryPathFinder.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

void Server::SetupLogger(const std::string_view serviceName, const std::string_view folder) {
	if (Game::logger) {
		LOG("A logger has already been setup, skipping.");
		return;
	}

	auto logsDir = BinaryPathFinder::GetBinaryDir() / "logs";
	if (!folder.empty()) logsDir /= folder;

	if (!std::filesystem::exists(logsDir)) std::filesystem::create_directories(logsDir);

	std::string logPath = (logsDir / serviceName).string() + ".log";
	bool logToConsole = false;
	bool logDebugStatements = false;
#ifdef _DEBUG
	logToConsole = true;
	logDebugStatements = true;
#endif
	Game::logger = new Logger(logPath, logToConsole, logDebugStatements);

	Game::logger->SetLogToConsole(Game::config->GetValue("log_to_console") != "0");
	Game::logger->SetLogDebugStatements(Game::config->GetValue("log_debug_statements") == "1");
}
