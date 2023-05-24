#include "Logger.h"

#include <ctime>
#include <cstdarg>
#include <iostream>
#include <vector>

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
inline void _LogOut(std::ostream& out, const char* timeStr, const char* location, const char* logLevel, const char* message) {
	out << '[' << timeStr << ' ' << location << "] " << logLevel << message << '\n';
}

void Logger::_Log(const char* location, Logger::Level level, const char* format, ...) {
	va_list args;
	va_start(args, format);
	_Log(location, level, format, args);
	va_end(args);
}

void Logger::_Log(const char* location, Logger::Level logLevel, const char* format, va_list args) {
	static std::vector<std::string> fileLogLevel = {
		"", // INFO
		"WARNING: ", // WARNING
		"ERROR: "  // ERROR
	};

	static std::vector<std::string> consoleLogLevel{
		"", // INFO
		"\033[1;33mWARNING: \033[0m", // WARNING
		"\033[1;31mERROR: \033[0m"  // ERROR
	};
	time_t t = time(NULL);
	struct tm* time = localtime(&t);
	char timeStr[70];
	strftime(timeStr, 70, "%d-%m-%y %H:%M:%S", time);
	char message[2048];
	vsnprintf(message, 2048, format, args);
	if (m_logToConsole) _LogOut(std::cout, timeStr, location, consoleLogLevel.at(logLevel).c_str(), message);
	[[likely]] if (m_File.good()) _LogOut(m_File, timeStr, location, fileLogLevel.at(logLevel).c_str(), message);
	else std::cout << "Log file is not in a good state and cannot be written to.\n";
}
