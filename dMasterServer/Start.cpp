#include "Start.h"
#include "Logger.h"
#include "dConfig.h"
#include "Game.h"
#include "BinaryPathFinder.h"

#ifdef _WIN32
#include <windows.h>
#include <handleapi.h>
#include <processthreadsapi.h>

namespace {
	const auto startup = STARTUPINFOW{
		.cb = sizeof(STARTUPINFOW),
		.lpReserved = nullptr,
		.lpDesktop = nullptr,
		.lpTitle = nullptr,
		.dwX = 0,
		.dwY = 0,
		.dwXSize = 0,
		.dwYSize = 0,
		.dwXCountChars = 0,
		.dwYCountChars = 0,
		.dwFillAttribute = 0,
		.dwFlags = 0,
		.wShowWindow = 0,
		.cbReserved2 = 0,
		.lpReserved2 = nullptr,
		.hStdInput = INVALID_HANDLE_VALUE,
		.hStdOutput = INVALID_HANDLE_VALUE,
		.hStdError = INVALID_HANDLE_VALUE,
	};
}
#else
#include <unistd.h>
#endif

uint32_t StartChatServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Chat will not be restarted.");
		return 0;
	}
	auto chat_path = BinaryPathFinder::GetBinaryDir() / "ChatServer";
#ifdef _WIN32
	chat_path.replace_extension(".exe");
    auto chat_startup = startup;
    auto chat_info = PROCESS_INFORMATION{};
	if (!CreateProcessW(chat_path.wstring().data(), chat_path.wstring().data(),
						nullptr, nullptr, false, 0, nullptr, nullptr,
						&chat_startup, &chat_info))
	{
		LOG("Failed to launch ChatServer");
		return 0;
	}

	// get pid and close unused handles
	auto chat_pid = chat_info.dwProcessId;
    CloseHandle(chat_info.hProcess);
	CloseHandle(chat_info.hThread);
#else // *nix systems
	const auto chat_pid = fork();
	if (chat_pid < 0) {
		LOG("Failed to launch ChatServer");
		return 0;
	} else if (chat_pid == 0) {
		// We are the child process
		execl(chat_path.string().c_str(), chat_path.string().c_str(), nullptr);
	}
#endif
	LOG("ChatServer PID is %d", chat_pid);
	return chat_pid;
}

uint32_t StartAuthServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Auth will not be restarted.");
		return 0;
	}
	auto auth_path = BinaryPathFinder::GetBinaryDir() / "AuthServer";
#ifdef _WIN32
	auth_path.replace_extension(".exe");
	auto auth_startup = startup;
    auto auth_info = PROCESS_INFORMATION{};
    if (!CreateProcessW(auth_path.wstring().data(), auth_path.wstring().data(),
						nullptr, nullptr, false, 0, nullptr, nullptr,
						&auth_startup, &auth_info))
	{
        LOG("Failed to launch AuthServer");
		return 0;
    }

    // get pid and close unused handles
	auto auth_pid = auth_info.dwProcessId;
    CloseHandle(auth_info.hProcess);
    CloseHandle(auth_info.hThread);
#else // *nix systems
	const auto auth_pid = fork();
	if (auth_pid < 0) {
		LOG("Failed to launch AuthServer");
		return 0;
	} else if (auth_pid == 0) {
		// We are the child process
		execl(auth_path.string().c_str(), auth_path.string().c_str(), nullptr);
	}
#endif
	LOG("AuthServer PID is %d", auth_pid);
	return auth_pid;
}

uint32_t StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID lastInstanceID, int maxPlayers, LWOCLONEID cloneID) {
	auto world_path = BinaryPathFinder::GetBinaryDir() / "WorldServer";
#ifdef _WIN32
	world_path.replace_extension(".exe");
	auto cmd = world_path.wstring() + L" -zone " + std::to_wstring(mapID) + L" -port " + std::to_wstring(port) +
		L" -instance " + std::to_wstring(lastInstanceID) + L" -maxclients " + std::to_wstring(maxPlayers) +
		L" -clone " + std::to_wstring(cloneID);

	auto world_startup = startup;
	auto world_info = PROCESS_INFORMATION{};
	if (!CreateProcessW(world_path.wstring().data(), cmd.data(),
						nullptr, nullptr, false, 0, nullptr, nullptr,
						&world_startup, &world_info))
	{
		LOG("Failed to launch WorldServer");
		return 0;
	}

	// get pid and close unused handles
	auto world_pid = world_info.dwProcessId;
	CloseHandle(world_info.hProcess);
	CloseHandle(world_info.hThread);
#else
	const auto world_pid = fork();
	if (world_pid < 0) {
		LOG("Failed to launch WorldServer");
		return 0;
	} else if (world_pid == 0) {
		// We are the child process
		execl(world_path.string().c_str(), world_path.string().c_str(),
			"-zone", std::to_string(mapID).c_str(),
			"-port", std::to_string(port).c_str(),
			"-instance", std::to_string(lastInstanceID).c_str(),
			"-maxclients", std::to_string(maxPlayers).c_str(),
			"-clone", std::to_string(cloneID).c_str(), nullptr);
	}
#endif
	LOG("WorldServer PID is %d", world_pid);
	return world_pid;
}
