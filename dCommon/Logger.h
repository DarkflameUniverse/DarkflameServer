#pragma once

#include <string>
#include <vector>

#define STRINGIFY_IMPL(x) #x

#define STRINGIFY(x) STRINGIFY_IMPL(x)

#define GET_FILE_NAME(x, y) GetFileNameFromAbsolutePath(__FILE__ x y)

#define FILENAME_AND_LINE GET_FILE_NAME(":", STRINGIFY(__LINE__))

// Calculate the filename at compile time from the path.
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

#define LOG(message, ...) do { constexpr auto str = FILENAME_AND_LINE; Game::logger->Log(str, message, ##__VA_ARGS__); } while(0);
#define LOG_DEBUG(message, ...) do { constexpr auto str = FILENAME_AND_LINE; Game::logger->LogDebug(str, message, ##__VA_ARGS__); } while(0);

// Writer class for writing data.

enum class WriterType : uint32_t {
	Writer,
	FileWriter,
	ConsoleWriter,
	UnixConsoleWriter,
	WindowsConsoleWriter
};

class Writer {
public:
	Writer(bool enabled = true) : m_Enabled(enabled) {};
	virtual ~Writer() = default;

	virtual void Log(const char* time, const char* message) = 0;
	virtual void Flush() {};

	void SetEnabled(bool disabled) { m_Enabled = disabled; }
	bool GetEnabled() const { return m_Enabled; }

	virtual WriterType GetType() { return WriterType::Writer; }

	bool IsConsoleWriter() {
		return GetType() == WriterType::ConsoleWriter || GetType() == WriterType::UnixConsoleWriter || GetType() == WriterType::WindowsConsoleWriter;
	}
public:
	bool m_Enabled = true;
};

// FileWriter class for writing data to a file on a disk.
class FileWriter : public Writer {
public:
	FileWriter(const char* outpath);
	FileWriter(const std::string& outpath) : FileWriter(outpath.c_str()) {};

	virtual ~FileWriter() override;

	void Log(const char* time, const char* message) override;
	void Flush() override;

	WriterType GetType() override { return WriterType::FileWriter; }
protected:
	std::string m_Outpath;
	FILE* m_Outfile;
};

// ConsoleWriter class for writing data to the console.
class ConsoleWriter : public Writer {
public:
	ConsoleWriter(bool enabled) : Writer(enabled) {};
	void Log(const char* time, const char* message) override;

	WriterType GetType() override { return WriterType::ConsoleWriter; }
};

// UnixConsoleWriter class for writing data to the console on Unix systems.
class UnixConsoleWriter : public ConsoleWriter {
public:
	UnixConsoleWriter(bool enabled) : ConsoleWriter(enabled) {};
	void Log(const char* time, const char* message) override {};

	WriterType GetType() override { return WriterType::UnixConsoleWriter; }
};

// WindowsConsoleWriter class for writing data to the console on Windows systems.
class WindowsConsoleWriter : public ConsoleWriter {
public:
	WindowsConsoleWriter(bool enabled) : ConsoleWriter(enabled) {};
	void Log(const char* time, const char* message) override {};

	WriterType GetType() override { return WriterType::WindowsConsoleWriter; }
};

class Logger {
public:
	Logger() = delete;
	Logger(const std::string& outpath, bool logToConsole, bool logDebugStatements);
	~Logger();

	void Log(const char* filenameAndLine, const char* format, ...);
	void LogDebug(const char* filenameAndLine, const char* format, ...);

	void Flush();

	bool GetLogToConsole() const;
	void SetLogToConsole(bool logToConsole);

	void SetLogDebugStatements(bool logDebugStatements) { m_logDebugStatements = logDebugStatements; }

private:
	void vLog(const char* format, va_list args);

	bool m_logDebugStatements;
	std::vector<Writer*> m_Writers;
};
