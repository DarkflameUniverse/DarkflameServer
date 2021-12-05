#pragma once
#include <ctime>
#include <cstdarg>
#include <string>
#include <fstream>
#include <iostream>

class dLogger {
public:
	dLogger(const std::string& outpath, bool logToConsole);
	~dLogger();

	void SetLogToConsole(bool logToConsole) { m_logToConsole = logToConsole; }

	void LogBasic(const std::string& message);
	void LogBasic(const char* format, ...);
	void Log(const char* className, const char* format, ...);
	void Log(const std::string& className, const std::string& message);

	void Flush();

	const bool GetIsLoggingToConsole() const { return m_logToConsole; }

private:
	bool m_logToConsole;
	std::string m_outpath;
	std::ofstream mFile;

	#ifndef _WIN32
		//Glorious linux can run with SPEED:
		FILE* fp = nullptr;
	#endif
};