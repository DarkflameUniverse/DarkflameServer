#ifndef __LOGGER__H__
#define __LOGGER__H__

#include <ctime>
#include <cstdarg>
#include <string>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "Singleton.h"

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

// Logging macros.  Use these when you want to log something

// We could #define the LogDebug away, but you may want to turn it on mid gameplay, so we'll just check the logger's debug flag instead.
#define LogDebug(message, ...) if (Logger::Instance().GetLogDebug()) { Logger::Instance()._Log(FILENAME, message, ##__VA_ARGS__); }

#define FlushLog Logger::Instance().Flush()
#define Log(message, ...) Logger::Instance()._Log(FILENAME, message, ##__VA_ARGS__)
#define LogWarning(message, ...) Logger::Instance()._LogWarning(FILENAME, message, ##__VA_ARGS__)
#define LogError(message, ...) Logger::Instance()._LogError(FILENAME, message, ##__VA_ARGS__)

class Logger : public Singleton<Logger> {
private:
	enum LogLevel : uint32_t {
		INFO,
		WARNING,
		ERROR,
		NUM_LEVELS
	};

public:
	~Logger();

	void Initialize(const std::string& outpath, bool logToConsole, bool logDebugStatements);
	void _Log(const char* location, const char* format, ...);
	void _LogWarning(const char* location, const char* format, ...);
	void _LogError(const char* location, const char* format, ...);

	const bool GetLogToConsole() const { return m_logToConsole; }
	void SetLogToConsole(bool logToConsole) { m_logToConsole = logToConsole; }

	const bool GetLogDebug() const { return m_logDebugStatements; }
	void SetLogDebug(bool logDebugStatements) { m_logDebugStatements = logDebugStatements; }
	inline void Flush() { m_File.flush(); };
private:
	void _Log(const char* location, const char* format, LogLevel logLevel, va_list args);

	bool m_logToConsole;
	bool m_logDebugStatements;
	std::ofstream m_File;
};
#endif  //!__LOGGER__H__
