#include "Logger.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <stdarg.h>

Writer::~Writer() {
	// Flush before we close
	Flush();
	// Dont try to close stdcout...
	if (!m_Outfile || m_IsConsoleWriter) return;

	fclose(m_Outfile);
	m_Outfile = NULL;
}

void Writer::Log(const char* time, const char* message) {
	if (!m_Outfile || !m_Enabled) return;

	fputs(time, m_Outfile);
	fputs(message, m_Outfile);
}

void Writer::Flush() {
	if (!m_Outfile) return;
	fflush(m_Outfile);
}

FileWriter::FileWriter(const char* outpath) {
	m_Outfile = fopen(outpath, "wt");
	if (!m_Outfile) printf("Couldn't open %s for writing!\n", outpath);
	m_Outpath = outpath;
	m_IsConsoleWriter = false;
}

ConsoleWriter::ConsoleWriter(bool enabled) {
	m_Enabled = enabled;
	m_Outfile = stdout;
	m_IsConsoleWriter = true;
}

Logger::Logger(const std::string& outpath, bool logToConsole, bool logDebugStatements) {
	m_logDebugStatements = logDebugStatements;
	std::filesystem::path outpathPath(outpath);
	if (!std::filesystem::exists(outpathPath.parent_path())) std::filesystem::create_directories(outpathPath.parent_path());
	m_Writers.push_back(std::make_unique<FileWriter>(outpath));
	m_Writers.push_back(std::make_unique<ConsoleWriter>(logToConsole));
}

void Logger::vLog(const char* format, va_list args) {
	time_t t = time(NULL);
	struct tm* time = localtime(&t);
	char timeStr[70];
	strftime(timeStr, sizeof(timeStr), "[%d-%m-%y %H:%M:%S ", time);
	char message[2048];
	vsnprintf(message, 2048, format, args);
	for (const auto& writer : m_Writers) {
		writer->Log(timeStr, message);
	}
}

void Logger::Log(const char* className, const char* format, ...) {
	va_list args;
	std::string log = std::string(className) + "] " + std::string(format) + "\n";
	va_start(args, format);
	vLog(log.c_str(), args);
	va_end(args);
}

void Logger::LogDebug(const char* className, const char* format, ...) {
	if (!m_logDebugStatements) return;
	va_list args;
	std::string log = std::string(className) + "] " + std::string(format) + "\n";
	va_start(args, format);
	vLog(log.c_str(), args);
	va_end(args);
}

void Logger::Flush() {
	for (const auto& writer : m_Writers) {
		writer->Flush();
	}
}

void Logger::SetLogToConsole(bool logToConsole) {
	for (const auto& writer : m_Writers) {
		if (writer->IsConsoleWriter()) writer->SetEnabled(logToConsole);
	}
}

bool Logger::GetLogToConsole() const {
	bool toReturn = false;
	for (const auto& writer : m_Writers) {
		if (writer->IsConsoleWriter()) toReturn |= writer->GetEnabled();
	}
	return toReturn;
}
