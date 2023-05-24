#ifndef __LOGGER__H__
#define __LOGGER__H__

#include <string>
#include <fstream>

// FIXME: The stringify macros are defined in dCommonVars but i want to keep this header small.
#define STRINGIFY_IMPL(x) #x

#define STRINGIFY(x) STRINGIFY_IMPL(x)

#define GET_FILE_NAME(x, y) GetFileName(__FILE__ x y)

#define FILENAME GET_FILE_NAME(":", STRINGIFY(__LINE__))

// Calculate the filename at compile time from the path.
constexpr char* GetFileName(const char* path) {
	const char* filename = path;
	for (const char* p = path; *p; ++p) {
		if (*p == '/' || *p == '\\') {
			filename = p + 1;
		}
	}
	return (char*)filename;
}

// We could #define the LogDebugs away, but you may want to turn it on mid gameplay, so we'll just check the logger's debug flag instead.

#define Log(message, ...) Game::logger->_Log(FILENAME, Logger::Info, message, ##__VA_ARGS__)
#define LogDebug(message, ...) if (Game::logger->GetLogDebug()) { Game::logger->_Log(FILENAME, Logger::Info, message, ##__VA_ARGS__); }

// Log a warning message. This is for when something unexpected happens, but the game can continue.  This log will print WARNING to the console in yellow.
#define LogWarning(message, ...) Game::logger->_Log(FILENAME, Logger::Warning, message, ##__VA_ARGS__)
#define LogDebugWarning(message, ...) if (Game::logger->GetLogDebug()) { Game::logger->_Log(FILENAME, Logger::Warning, message, ##__VA_ARGS__); }

// Log an error message. This is for when something unexpected happens and the game likely cannot continue. This log will print ERROR to the console in red.
#define LogError(message, ...) Game::logger->_Log(FILENAME, Logger::Error, message, ##__VA_ARGS__)
#define LogDebugError(message, ...) if (Game::logger->GetLogDebug()) { Game::logger->_Log(FILENAME, Logger::Error, message, ##__VA_ARGS__); }

#define FlushLog Game::logger->Flush()
class Logger {
public:
	enum Level : uint32_t {
		Info,
		Warning,
		Error,
		NUM_LEVELS
	};

	Logger(const std::string& outpath, const bool logToConsole, const bool logDebugStatements);
	~Logger();

	const bool GetLogToConsole() const { return m_logToConsole; }
	void SetLogToConsole(bool logToConsole) { m_logToConsole = logToConsole; }

	const bool GetLogDebug() const { return m_logDebugStatements; }
	void SetLogDebug(bool logDebugStatements) { m_logDebugStatements = logDebugStatements; }

	inline void Flush() { m_File.flush(); };
	void _Log(const char* location, const Logger::Level level, const char* format, ...);
private:
	void _Log(const char* location, const Logger::Level logLevel, const char* format, va_list args);

	bool m_logToConsole;
	bool m_logDebugStatements;
	std::ofstream m_File;
};
#endif  //!__LOGGER__H__
