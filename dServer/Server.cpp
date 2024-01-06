#include "Server.h"

#include "BinaryPathFinder.h"
#include "Game.h"
#include "Logger.h"
#include "dConfig.h"

void Server::SetupLogger(const std::string_view serviceName) {
	if (Game::logger) {
		LOG("A logger has already been setup, skipping.");
		return;
	}

	const auto logsDir = BinaryPathFinder::GetBinaryDir() / "logs";

	if (!std::filesystem::exists(logsDir)) std::filesystem::create_directory(logsDir);

	std::string logPath = (logsDir / serviceName).string() + "_" + std::to_string(time(nullptr)) + ".log";
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
