#include "Logger.h"

#include <ctime>
#include <cstdarg>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

Logger::Logger(const std::string& outpath, const bool logToConsole, const bool logDebugStatements) {
	m_logToConsole = logToConsole;
	m_logDebugStatements = logDebugStatements;
	m_File = std::ofstream(outpath);
}

Logger::~Logger() {
	m_File.flush();
}

// Helper function so what we log to stdout and the file is the same.
// Yes we could move that space to the strftime call, but this is more readable.
// I'm just going to make a separate function for windows lol.
inline void _LogOut(std::ostream& out, const char* timeStr, const char* location, const char* logLevel, const char* message) {
#ifdef  _WIN32
	// Save the current color, print the time and location, then restore the color.
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hStdout, &csbi);
	auto prevColor = csbi.wAttributes;
	// print time in white
	SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	out << '[' << timeStr << ' ' << location << "] ";
	// print log level in color
	SetConsoleTextAttribute(hStdout, prevColor);
	out << logLevel;
	SetConsoleTextAttribute(hStdout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	out << message << '\n';
#else
	out << '[' << timeStr << ' ' << location << "] " << logLevel << message << '\n';
#endif
}

void Logger::_Log(const char* location, const Logger::Level level, const char* format, ...) {
	va_list args;
	va_start(args, format);
	_Log(location, level, format, args);
	va_end(args);
}

void Logger::_Log(const char* location, const Logger::Level logLevel, const char* format, va_list args) {

	static std::vector<std::string> fileLogLevel = {
		"", // INFO
		"WARNING: ", // WARNING
		"ERROR: "  // ERROR
	};
#ifndef _WIN32
	static std::vector<std::string> consoleLogLevel{
		"", // INFO
		"\033[1;33mWARNING: \033[0m", // WARNING
		"\033[1;31mERROR: \033[0m"  // ERROR
	};
#else
	auto stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	// Unpredictable branches in my hot path?  Never...
	if (logLevel == Logger::Level::Warning) SetConsoleTextAttribute(stdHandle, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
	else if (logLevel == Logger::Level::Error) SetConsoleTextAttribute(stdHandle, FOREGROUND_RED | FOREGROUND_INTENSITY);
#endif
	time_t t = time(NULL);
	struct tm* time = localtime(&t);
	char timeStr[70];
	strftime(timeStr, 70, "%d-%m-%y %H:%M:%S", time);
	char message[2048];
	vsnprintf(message, 2048, format, args);
#ifndef _WIN32
	if (m_logToConsole) _LogOut(std::cout, timeStr, location, consoleLogLevel.at(logLevel).c_str(), message);
#else
	if (m_logToConsole) _LogOut(std::cout, timeStr, location, fileLogLevel.at(logLevel).c_str(), message);
#endif
	SetConsoleTextAttribute(stdHandle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	[[likely]] if (m_File.good()) _LogOut(m_File, timeStr, location, fileLogLevel.at(logLevel).c_str(), message);
	else std::cout << "Log file is not in a good state and cannot be written to.\n";
}
