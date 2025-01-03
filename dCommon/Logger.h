#pragma once

#include <memory>
#include <string>
#include <vector>

#define STRINGIFY_IMPL(x) #x

#define STRINGIFY(x) STRINGIFY_IMPL(x)

#define GET_FILE_NAME(x, y) GetFileNameFromAbsolutePath(__FILE__ x y)

#define FILENAME_AND_LINE GET_FILE_NAME(":", STRINGIFY(__LINE__))

// Calculate the filename at compile time from the path.
// We just do this by scanning the path for the last '/' or '\' character and returning the string after it.
constexpr const char* GetFileNameFromAbsolutePath(const char* path) {
    const char* file = path;
    while (*path) {
		char nextChar = *path++;
        if (nextChar == '/' || nextChar == '\\') {
            file = path;
        }
    }
    return file;
}

// These have to have a constexpr variable to store the filename_and_line result in a local variable otherwise
// they will not be valid constexpr and will be evaluated at runtime instead of compile time!
// The full string is still stored in the binary, however the offset of the filename in the absolute paths
// is used in the instruction instead of the start of the absolute path.
#define LOG(message, ...) do { auto str = FILENAME_AND_LINE; Game::logger->Log(str, message, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(message, ...) do { auto str = FILENAME_AND_LINE; Game::logger->LogDebug(str, message, ##__VA_ARGS__); } while(0)

// Writer class for writing data to files.
class Writer {
public:
	Writer(bool enabled = true) : m_Enabled(enabled) {};
	virtual ~Writer();

	virtual void Log(const char* time, const char* message);
	virtual void Flush();

	void SetEnabled(bool disabled) { m_Enabled = disabled; }
	bool GetEnabled() const { return m_Enabled; }

	bool IsConsoleWriter() { return m_IsConsoleWriter; }
public:
	bool m_Enabled = true;
	bool m_IsConsoleWriter = false;
	FILE* m_Outfile;
};

// FileWriter class for writing data to a file on a disk.
class FileWriter : public Writer {
public:
	FileWriter(const char* outpath);
	FileWriter(const std::string& outpath) : FileWriter(outpath.c_str()) {};
private:
	std::string m_Outpath;
};

// ConsoleWriter class for writing data to the console.
class ConsoleWriter : public Writer {
public:
	ConsoleWriter(bool enabled);
};

class Logger {
public:
	Logger() = delete;
	Logger(const std::string& outpath, bool logToConsole, bool logDebugStatements);

	void Log(const char* filenameAndLine, const char* format, ...);
	void LogDebug(const char* filenameAndLine, const char* format, ...);

	void Flush();

	bool GetLogToConsole() const;
	void SetLogToConsole(bool logToConsole);

	void SetLogDebugStatements(bool logDebugStatements) { m_logDebugStatements = logDebugStatements; }

private:
	void vLog(const char* format, va_list args);

	bool m_logDebugStatements;
	std::vector<std::unique_ptr<Writer>> m_Writers;
};
