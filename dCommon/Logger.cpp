#include "Logger.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <stdarg.h>

FileWriter::FileWriter(const char* outpath) {
	m_Outfile = fopen(outpath, "wt");
	if (m_Outfile == NULL) printf("Couldn't open %s for writing!\n", outpath);
	m_Outpath = outpath;
}

FileWriter::~FileWriter() {
	if (m_Outfile == NULL) return;

	fclose(m_Outfile);
	m_Outfile = NULL;
}

void FileWriter::Log(const char* time, const char* message) {
	if (m_Outfile == NULL) return;

	fputs(time, m_Outfile);
	fputs(message, m_Outfile);
}

void FileWriter::Flush() {
	if (m_Outfile == NULL) return;
	fflush(m_Outfile);
}

ConsoleWriter::~ConsoleWriter() {
	fflush(stdout);
}

void ConsoleWriter::Log(const char* time, const char* message) {
	fputs(time, stdout);
	fputs(message, stdout);
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
	std::for_each(m_Writers.begin(), m_Writers.end(), [&](const std::unique_ptr<Writer>& writer) {
		writer->Log(timeStr, message);
		});
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
	std::for_each(m_Writers.begin(), m_Writers.end(), [](const std::unique_ptr<Writer>& writer) {
		writer->Flush();
		});
}

void Logger::SetLogToConsole(bool logToConsole) {
	std::for_each(m_Writers.begin(), m_Writers.end(), [&](const std::unique_ptr<Writer>& writer) {
		if (writer->IsConsoleWriter()) writer->SetEnabled(logToConsole);
		});
}

bool Logger::GetLogToConsole() const {
	return std::find_if(m_Writers.begin(), m_Writers.end(), [](const std::unique_ptr<Writer>& writer) {
		if (writer->IsConsoleWriter()) return writer->GetEnabled();
		return false;
		}) != m_Writers.end();
}
