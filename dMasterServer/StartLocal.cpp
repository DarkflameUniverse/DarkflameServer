#include <windows.h>
#include <iostream>
#include <string>
#include "Start.h"
#include "Logger.h"
#include "dConfig.h"
#include "Game.h"
#include "BinaryPathFinder.h"

// Function to create a process and execute a DLL
bool RunDLLAsApplication(const char* dllPath) {
	STARTUPINFOA si = { sizeof(STARTUPINFOA) };
	PROCESS_INFORMATION pi = { 0 };

	// Path to a dummy executable
	const char* dummyProcessPath = "C:\\Windows\\System32\\notepad.exe";

	// Create a suspended dummy process
	if (!CreateProcessA(
		dummyProcessPath,
		NULL,
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED,
		NULL,
		NULL,
		&si,
		&pi)) {
		std::cerr << "Failed to create dummy process. Error: " << GetLastError() << std::endl;
		return false;
	}

	// Get the address of LoadLibraryA in the current process
	HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
	if (!kernel32) {
		std::cerr << "Failed to get handle for kernel32.dll. Error: " << GetLastError() << std::endl;
		TerminateProcess(pi.hProcess, 1);
		return false;
	}

	void* loadLibraryAddr = (void*)GetProcAddress(kernel32, "LoadLibraryA");
	if (!loadLibraryAddr) {
		std::cerr << "Failed to get address of LoadLibraryA. Error: " << GetLastError() << std::endl;
		TerminateProcess(pi.hProcess, 1);
		return false;
	}

	// Allocate memory in the target process for the DLL path
	void* remoteMemory = VirtualAllocEx(pi.hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!remoteMemory) {
		std::cerr << "Failed to allocate memory in target process. Error: " << GetLastError() << std::endl;
		TerminateProcess(pi.hProcess, 1);
		return false;
	}

	// Write the DLL path into the allocated memory
	if (!WriteProcessMemory(pi.hProcess, remoteMemory, dllPath, strlen(dllPath) + 1, NULL)) {
		std::cerr << "Failed to write DLL path to target process. Error: " << GetLastError() << std::endl;
		VirtualFreeEx(pi.hProcess, remoteMemory, 0, MEM_RELEASE);
		TerminateProcess(pi.hProcess, 1);
		return false;
	}

	// Create a remote thread in the target process to load the DLL
	HANDLE remoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMemory, 0, NULL);
	if (!remoteThread) {
		std::cerr << "Failed to create remote thread. Error: " << GetLastError() << std::endl;
		VirtualFreeEx(pi.hProcess, remoteMemory, 0, MEM_RELEASE);
		TerminateProcess(pi.hProcess, 1);
		return false;
	}

	// Wait for the remote thread to complete
	WaitForSingleObject(remoteThread, INFINITE);
	CloseHandle(remoteThread);

	// Free the allocated memory
	VirtualFreeEx(pi.hProcess, remoteMemory, 0, MEM_RELEASE);

	// Resume the main thread of the process
	ResumeThread(pi.hThread);

	// Clean up process and thread handles
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	return true;
}

void StartChatServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Chat will not be restarted.");
		return;
	}

	RunDLLAsApplication("ChatServer.dll");
}

void StartAuthServer() {
	if (Game::ShouldShutdown()) {
		LOG("Currently shutting down.  Auth will not be restarted.");
		return;
	}

	RunDLLAsApplication("AuthServer.dll");
}

void StartWorldServer(LWOMAPID mapID, uint16_t port, LWOINSTANCEID lastInstanceID, int maxPlayers, LWOCLONEID cloneID) {
	RunDLLAsApplication("WorldServer.dll");

	//cmd.append(std::to_string(mapID));
	//cmd.append(" -port ");
	//cmd.append(std::to_string(port));
	//cmd.append(" -instance ");
	//cmd.append(std::to_string(lastInstanceID));
	//cmd.append(" -maxclients ");
	//cmd.append(std::to_string(maxPlayers));
	//cmd.append(" -clone ");
	//cmd.append(std::to_string(cloneID));
}
