#include "Start.h"
#include "Logger.h"
#include "dConfig.h"
#include "Game.h"
#include "BinaryPathFinder.h"

void StartChatServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Chat will not be restarted.");
		return;
	}
#ifdef __APPLE__
	//macOS doesn't need sudo to run on ports < 1024
	auto result = system(((BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
#elif _WIN32
	auto result = system(("start " + (BinaryPathFinder::GetBinaryDir() / "ChatServer.exe").string()).c_str());
#else
	if (std::atoi(Game::config->GetValue("use_sudo_chat").c_str())) {
		auto result = system(("sudo " + (BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
	} else {
		auto result = system(((BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
}
#endif
}

void StartAuthServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Auth will not be restarted.");
		return;
	}
#ifdef __APPLE__
	auto result = system(((BinaryPathFinder::GetBinaryDir() / "AuthServer").string() + "&").c_str());
#elif _WIN32
	auto result = system(("start " + (BinaryPathFinder::GetBinaryDir() / "AuthServer.exe").string()).c_str());
#else
	if (std::atoi(Game::config->GetValue("use_sudo_auth").c_str())) {
		auto result = system(("sudo " + (BinaryPathFinder::GetBinaryDir() / "AuthServer").string() + "&").c_str());
	} else {
		auto result = system(((BinaryPathFinder::GetBinaryDir() / "AuthServer").string() + "&").c_str());
}
#endif
}

void StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID lastInstanceID, int maxPlayers, LWOCLONEID cloneID) {
#ifdef _WIN32
	std::string cmd = "start " + (BinaryPathFinder::GetBinaryDir() / "WorldServer.exe").string() + " -zone ";
#else
	std::string cmd;
	if (std::atoi(Game::config->GetValue("use_sudo_world").c_str())) {
		cmd = "sudo " + (BinaryPathFinder::GetBinaryDir() / "WorldServer").string() + " -zone ";
	} else {
		cmd = (BinaryPathFinder::GetBinaryDir() / "WorldServer").string() + " -zone ";
	}
#endif

	cmd.append(std::to_string(mapID));
	cmd.append(" -port ");
	cmd.append(std::to_string(port));
	cmd.append(" -instance ");
	cmd.append(std::to_string(lastInstanceID));
	cmd.append(" -maxclients ");
	cmd.append(std::to_string(maxPlayers));
	cmd.append(" -clone ");
	cmd.append(std::to_string(cloneID));

#ifndef _WIN32
	cmd.append("&"); //Sends our next process to the background on Linux
#endif

	auto ret = system(cmd.c_str());
}
