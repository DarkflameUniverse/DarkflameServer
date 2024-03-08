#pragma once

// fmt includes:
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/chrono.h>

// C++ includes:
#include <chrono>
#include <memory>
#include <source_location>
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

/**
 * Location wrapper class
 * Used to implicitly forward source location information without adding a function parameter
*/
template <typename T>
class location_wrapper {
public:
	// Constructor
	template <std::convertible_to<T> U = T>
	consteval location_wrapper(const U val, const std::source_location loc = std::source_location::current())
		: m_Obj(val)
		, m_Loc(loc) {
	}

	// Methods
	[[nodiscard]] constexpr const T& get() const noexcept { return m_Obj; }

	[[nodiscard]] constexpr const std::source_location& loc() const noexcept { return m_Loc; }

	// Operator overloads
	location_wrapper& operator=(const location_wrapper& other) = default;

	constexpr operator T& () const noexcept { return get(); }

protected:
	T m_Obj{};
	std::source_location m_Loc{};
};

/**
 * Logging functions (EXPERIMENTAL)
*/
namespace Log {
	template <typename... Ts>
	constexpr tm Time() { // TODO: Move?
		return fmt::localtime(std::time(nullptr));
	}

	template <typename... Ts>
	constexpr void Info(const location_wrapper<fmt::format_string<Ts...>> fmt_str, Ts&&... args) {
		const auto filename = GetFileNameFromAbsolutePath(fmt_str.loc().file_name());

		fmt::print("[{:%d-%m-%y %H:%M:%S} {:s}:{:d}] ", Time(), filename, fmt_str.loc().line());
		fmt::println(fmt_str.get(), std::forward<Ts>(args)...);
	}

	template <typename... Ts>
	constexpr void Warn(const location_wrapper<fmt::format_string<Ts...>> fmt_str, Ts&&... args) {
		const auto filename = GetFileNameFromAbsolutePath(fmt_str.loc().file_name());

		fmt::print("[{:%d-%m-%y %H:%M:%S} {:s}:{:d}] Warning: ", Time(), filename, fmt_str.loc().line());
		fmt::println(fmt_str.get(), std::forward<Ts>(args)...);
	}

	template <typename... Ts>
	constexpr void Debug(const location_wrapper<fmt::format_string<Ts...>> fmt_str, Ts&&... args) {
		// if (!m_logDebugStatements) return;
		Log::Info(fmt_str, std::forward<Ts>(args)...);
	}
}

// These have to have a constexpr variable to store the filename_and_line result in a local variable otherwise
// they will not be valid constexpr and will be evaluated at runtime instead of compile time!
// The full string is still stored in the binary, however the offset of the filename in the absolute paths
// is used in the instruction instead of the start of the absolute path.
//#define LOG(message, ...) do { auto str = FILENAME_AND_LINE; Game::logger->Log(str, message, ##__VA_ARGS__); } while(0)
//#define LOG(message, ...) do {\
	const auto now = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());\
	fmt::println("[{:%d-%m-%y %H:%M:%S} {:s}] " message, now, FILENAME_AND_LINE, ##__VA_ARGS__);\
} while(0)
#define LOG(message, ...) Log::Info(message, ##__VA_ARGS__)

//#define LOG_DEBUG(message, ...) do { auto str = FILENAME_AND_LINE; Game::logger->LogDebug(str, message, ##__VA_ARGS__); } while(0)
#define LOG_DEBUG(message, ...) Log::Debug(message, ##__VA_ARGS__)

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
