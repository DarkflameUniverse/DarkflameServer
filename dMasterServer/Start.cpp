#include "Start.h"
#include "Logger.h"
#include "dConfig.h"
#include "Game.h"
#include "BinaryPathFinder.h"

#ifdef _WIN32

#endif

void StartChatServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Chat will not be restarted.");
		return;
	}
#ifdef __APPLE__
	//macOS doesn't need sudo to run on ports < 1024
	auto result = system(((BinaryPathFinder::GetBinaryDir() / "ChatServer").string() + "&").c_str());
#elif defined(_WIN32)
	auto result = system(("start /B " + (BinaryPathFinder::GetBinaryDir() / "ChatServer.exe").string()).c_str());
#else // *nix systems
	const auto chat_pid = fork();
	if (chat_pid < 0) {
		LOG("Failed to launch ChatServer");
	} else if (chat_pid == 0) {
		// We are the child process
 		LOG("ChatServer PID is %d", getpid());
		execl((BinaryPathFinder::GetBinaryDir() / "ChatServer").string().c_str(), "", nullptr);
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
	auto result = system(("start /B " + (BinaryPathFinder::GetBinaryDir() / "AuthServer.exe").string()).c_str());
#else // *nix systems
	const auto auth_pid = fork();
	if (auth_pid < 0) {
		LOG("Failed to launch AuthServer");
	} else if (auth_pid == 0) {
		// We are the child process
 		LOG("AuthServer PID is %d", getpid());
		execl((BinaryPathFinder::GetBinaryDir() / "AuthServer").string().c_str(), "", nullptr);
	}
#endif
}

void StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID lastInstanceID, int maxPlayers, LWOCLONEID cloneID) {
#ifdef _WIN32
	std::string cmd = "start /B " + (BinaryPathFinder::GetBinaryDir() / "WorldServer.exe").string() + " -zone ";
#else
	const auto world_pid = fork();
	if (world_pid < 0) {
		LOG("Failed to launch WorldServer");
	} else if (world_pid == 0) {
		// We are the child process
 		LOG("WorldServer PID is %d", getpid());
		execl((BinaryPathFinder::GetBinaryDir() / "WorldServer").string().c_str(),
			"-zone", std::to_string(mapID).c_str(),
			"-port", std::to_string(port).c_str(),
			"-instance", std::to_string(lastInstanceID).c_str(),
			"-maxclients", std::to_string(maxPlayers).c_str(),
			"-clone", std::to_string(cloneID).c_str(), nullptr);
	}
#endif
}
