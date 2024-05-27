#include "Diagnostics.h"
#include "Game.h"
#include "Logger.h"

// If we're on Win32, we'll include our minidump writer
#ifdef _WIN32

#include <Windows.h>
#include <Dbghelp.h>

#include "Game.h"
#include "Logger.h"

void make_minidump(EXCEPTION_POINTERS* e) {
	auto hDbgHelp = LoadLibraryA("dbghelp");
	if (hDbgHelp == nullptr)
		return;
	auto pMiniDumpWriteDump = (decltype(&MiniDumpWriteDump))GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
	if (pMiniDumpWriteDump == nullptr)
		return;

	char name[MAX_PATH];
	{
		auto nameEnd = name + GetModuleFileNameA(GetModuleHandleA(0), name, MAX_PATH);
		SYSTEMTIME t;
		GetSystemTime(&t);
		wsprintfA(nameEnd - strlen(".exe"),
			"_%4d%02d%02d_%02d%02d%02d.dmp",
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	}
	LOG("Creating crash dump %s", name);
	auto hFile = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
	exceptionInfo.ThreadId = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = e;
	exceptionInfo.ClientPointers = FALSE;

	auto dumped = pMiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
		e ? &exceptionInfo : nullptr,
		nullptr,
		nullptr);

	CloseHandle(hFile);

	return;
}

LONG CALLBACK unhandled_handler(EXCEPTION_POINTERS* e) {
	make_minidump(e);
	if (Game::logger)
		Game::logger->Flush(); // Flush our log if we have one, before exiting.
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

#if defined(__linux__) //&& !defined(__clang__) // backtrace is a gcc exclusive system library
#include <execinfo.h>
#include <ucontext.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#if defined(INCLUDE_BACKTRACE)
#include <backtrace.h>

#include <backtrace-supported.h>

struct bt_ctx {
	struct backtrace_state* state;
	int error;
};

static inline void Bt(struct backtrace_state* state) {
	std::string fileName = Diagnostics::GetOutDirectory() + "crash_" + Diagnostics::GetProcessName() + "_" + std::to_string(getpid()) + ".log";
	LOG("backtrace is enabled, crash dump located at %s", fileName.c_str());
	FILE* file = fopen(fileName.c_str(), "w+");
	if (file != nullptr) {
		backtrace_print(state, 2, file);
		fclose(file);
	}

	backtrace_print(state, 2, stdout);
}

static void ErrorCallback(void* data, const char* msg, int errnum) {
	auto* ctx = (struct bt_ctx*)data;
	fprintf(stderr, "ERROR: %s (%d)", msg, errnum);
	ctx->error = 1;

	std::string fileName = Diagnostics::GetOutDirectory() + "crash_" + Diagnostics::GetProcessName() + "_" + std::to_string(getpid()) + ".log";
	FILE* file = fopen(fileName.c_str(), "w+");
	if (file != nullptr) {
		fprintf(file, "ERROR: %s (%d)", msg, errnum);
		fclose(file);
	}
}
#endif

#include "Demangler.h"

void GenerateDump() {
	std::string cmd = "sudo gcore " + std::to_string(getpid());
	int ret = system(cmd.c_str()); // Saving a return just to prevent warning
}

void CatchUnhandled(int sig) {
	std::exception_ptr eptr = std::current_exception();
	try {
		if (eptr) std::rethrow_exception(eptr);
	} catch(const std::exception& e) {
		LOG("Caught exception: '%s'", e.what());
	} catch (...) {
		LOG("Caught unknown exception.");
	}

#ifndef INCLUDE_BACKTRACE

	std::string fileName = Diagnostics::GetOutDirectory() + "crash_" + Diagnostics::GetProcessName() + "_" + std::to_string(getpid()) + ".log";
	LOG("Encountered signal %i, creating crash dump %s", sig, fileName.c_str());
	if (Diagnostics::GetProduceMemoryDump()) {
		GenerateDump();
	}
	constexpr uint8_t MaxStackTrace = 32;
	void* array[MaxStackTrace];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, MaxStackTrace);

#  if defined(__GNUG__)

	// Loop through the returned addresses, and get the symbols to be demangled
	char** strings = backtrace_symbols(array, size);

	FILE* file = fopen(fileName.c_str(), "w+");
	if (file != NULL) {
		fprintf(file, "Error: signal %d:\n", sig);
	}
	// Print the stack trace
	for (size_t i = 0; i < size; i++) {
		// Take a string like './WorldServer(_ZN19SlashCommandHandler17HandleChatCommandERKSbIDsSt11char_traitsIDsESaIDsEEP6EntityRK13SystemAddress+0x6187) [0x55869c44ecf7]'
		// and extract '_ZN19SlashCommandHandler17HandleChatCommandERKSbIDsSt11char_traitsIDsESaIDsEEP6EntityRK13SystemAddress' from it to be demangled into a proper name
		std::string functionName = strings[i];
		std::string::size_type start = functionName.find('(');
		std::string::size_type end = functionName.find('+');
		if (start != std::string::npos && end != std::string::npos) {
			std::string demangled = functionName.substr(start + 1, end - start - 1);

			demangled = Demangler::Demangle(demangled.c_str());

			// If the demangled string is not empty, then we can replace the mangled string with the demangled one
			if (!demangled.empty()) {
				demangled.push_back('(');
				demangled += functionName.substr(end);
				functionName = demangled;
			}
		}

		LOG("[%02zu] %s", i, functionName.c_str());
		if (file != NULL) {
			fprintf(file, "[%02zu] %s\n", i, functionName.c_str());
		}
	}
#  else // defined(__GNUG__)
	backtrace_symbols_fd(array, size, STDOUT_FILENO);
#  endif // defined(__GNUG__)

#else // INCLUDE_BACKTRACE

	struct backtrace_state* state = backtrace_create_state(
		Diagnostics::GetProcessFileName().c_str(),
		BACKTRACE_SUPPORTS_THREADS,
		ErrorCallback,
		nullptr);

	struct bt_ctx ctx = { state, 0 };
	Bt(state);

#endif // INCLUDE_BACKTRACE

	exit(EXIT_FAILURE);
}

void CritErrHdlr(int sig_num, siginfo_t* info, void* ucontext) {
	CatchUnhandled(sig_num);
}

void OnTerminate() {
	CatchUnhandled(-1);
}

void MakeBacktrace() {
	struct sigaction sigact{};

	sigact.sa_sigaction = CritErrHdlr;
	sigact.sa_flags = SA_RESTART | SA_SIGINFO;

	if (sigaction(SIGSEGV, &sigact, nullptr) != 0 ||
		sigaction(SIGFPE, &sigact, nullptr) != 0 ||
		sigaction(SIGABRT, &sigact, nullptr) != 0 ||
		sigaction(SIGILL, &sigact, nullptr) != 0) {
		fprintf(stderr, "error setting signal handler for %d (%s)\n",
			SIGSEGV,
			strsignal(SIGSEGV));

		exit(EXIT_FAILURE);
	}

	std::set_terminate(OnTerminate);
}
#endif

void Diagnostics::Initialize() {
#ifdef _WIN32
	SetUnhandledExceptionFilter(unhandled_handler);
#elif defined(__linux__) //&& !defined(__clang__)
	MakeBacktrace();
#else
	fprintf(stderr, "Diagnostics not supported on this platform.\n");
#endif
}

std::string Diagnostics::m_ProcessName{};
std::string Diagnostics::m_ProcessFileName{};
std::string Diagnostics::m_OutDirectory{};
bool Diagnostics::m_ProduceMemoryDump{};

void Diagnostics::SetProcessName(const std::string& name) {
	m_ProcessName = name;
}

void Diagnostics::SetProcessFileName(const std::string& name) {
	m_ProcessFileName = name;
}

void Diagnostics::SetOutDirectory(const std::string& path) {
	m_OutDirectory = path;
}

void Diagnostics::SetProduceMemoryDump(bool value) {
	m_ProduceMemoryDump = value;
}

const std::string& Diagnostics::GetProcessName() {
	return m_ProcessName;
}

const std::string& Diagnostics::GetProcessFileName() {
	return m_ProcessFileName;
}

const std::string& Diagnostics::GetOutDirectory() {
	return m_OutDirectory;
}

bool Diagnostics::GetProduceMemoryDump() {
	return m_ProduceMemoryDump;
}
