#include "Logger.h"

Logger::~Logger() {
	if (m_File.good()) m_File.flush();
}

void Logger::Initialize(const std::string& outpath, bool logToConsole, bool logDebugStatements) {
	m_logToConsole = logToConsole;
	m_logDebugStatements = logDebugStatements;
	m_File = std::ofstream(outpath);
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
	char timeStr[70];
	strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", time);
	char message[2048];
	vsnprintf(message, 2048, format, args);
	if (m_logToConsole) {
		fputc('[', stdout);
		fputs(timeStr, stdout);
		fputc(' ', stdout);
		fputs(location, stdout);
		fputs("] ", stdout);
		fputs(consoleLogLevel.at(logLevel).c_str(), stdout);
		fputs(message, stdout);
		fputc('\n', stdout);
	}
	if (m_File.good()) m_File << '[' << timeStr << ' ' << location << "] " << fileLogLevel.at(logLevel) << message << '\n';
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
