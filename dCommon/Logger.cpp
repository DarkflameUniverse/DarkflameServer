#include "Logger.h"

#include <array>

Logger::Logger(const std::string& outpath, const bool logToConsole, const bool logDebugStatements) {
	m_logToConsole = logToConsole;
	m_logDebugStatements = logDebugStatements;
	m_File = std::ofstream(outpath);
}

Logger::~Logger() {
	m_File.flush();
}

void Logger::_Log(const char* location, const char* format, LogLevel logLevel, va_list args) {
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
	std::array<char, 70> timeStr;
	strftime(timeStr.begin(), timeStr.size(), "%d-%m-%y %H:%M:%S", time);
	std::array<char, 2048> message;
	vsnprintf(message.begin(), message.size(), format, args);
	if (m_logToConsole) {
		fputc('[', stdout);
		fputs(timeStr.begin(), stdout);
		fputc(' ', stdout);
		fputs(location, stdout);
		fputs("] ", stdout);
		fputs(consoleLogLevel.at(logLevel).c_str(), stdout);
		fputs(message.begin(), stdout);
		fputc('\n', stdout);
	}
	if (m_File.good()) m_File << '[' << timeStr.begin() << ' ' << location << "] " << fileLogLevel.at(logLevel) << message.begin() << '\n';
	else std::cout << "Log file is not in a good state and cannot be written to.\n";
}

void Logger::_Log(const char* location, const char* format, ...) {
	va_list args;
	va_start(args, format);
	_Log(location, format, LogLevel::INFO, args);
	va_end(args);
}

void Logger::_LogWarning(const char* location, const char* format, ...) {
	va_list args;
	va_start(args, format);
	_Log(location, format, LogLevel::WARNING, args);
	va_end(args);
}

void Logger::_LogError(const char* location, const char* format, ...) {
	va_list args;
	va_start(args, format);
	_Log(location, format, LogLevel::ERROR, args);
	va_end(args);
}
