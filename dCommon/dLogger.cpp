#include "dLogger.h"

dLogger::dLogger(const std::string& outpath, bool logToConsole, bool logDebugStatements) {
	m_logToConsole = logToConsole;
	m_logDebugStatements = logDebugStatements;
	m_outpath = outpath;

#ifdef _WIN32
	mFile = std::ofstream(m_outpath);
	if (!mFile) { printf("Couldn't open %s for writing!\n", outpath.c_str()); }
#else
	fp = fopen(outpath.c_str(), "wt");
	if (fp == NULL) { printf("Couldn't open %s for writing!\n", outpath.c_str()); }
#endif
}

dLogger::~dLogger() {
#ifdef _WIN32
	mFile.close();
#else
	if (fp != nullptr) {
		fclose(fp);
		fp = nullptr;
	}
#endif
}

void dLogger::vLog(const char* format, va_list args) {
#ifdef _WIN32
	time_t t = time(NULL);
	struct tm time;
	localtime_s(&time, &t);
	char timeStr[70];
	strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", &time);
	char message[2048];
	vsnprintf(message, 2048, format, args);

	if (m_logToConsole) std::cout << "[" << timeStr << "] " << message;
	mFile << "[" << timeStr << "] " << message;
#else
	time_t t = time(NULL);
	struct tm* time = localtime(&t);
	char timeStr[70];
	strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", time);
	char message[2048];
	vsnprintf(message, 2048, format, args);

	if (m_logToConsole) {
		fputs("[", stdout);
		fputs(timeStr, stdout);
		fputs("] ", stdout);
		fputs(message, stdout);
	}

	if (fp != nullptr) {
		fputs("[", fp);
		fputs(timeStr, fp);
		fputs("] ", fp);
		fputs(message, fp);
	} else {
		printf("Logger not initialized!\n");
	}
#endif
}

void dLogger::LogBasic(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vLog(format, args);
	va_end(args);
}

void dLogger::LogBasic(const std::string& message) {
	LogBasic(message.c_str());
}

void dLogger::Log(const char* className, const char* format, ...) {
	va_list args;
	std::string log = "[" + std::string(className) + "] " + std::string(format) + "\n";
	va_start(args, format);
	vLog(log.c_str(), args);
	va_end(args);
}

void dLogger::Log(const std::string& className, const std::string& message) {
	Log(className.c_str(), message.c_str());
}

void dLogger::LogDebug(const char* className, const char* format, ...) {
	if (!m_logDebugStatements) return;
	va_list args;
	std::string log = "[" + std::string(className) + "] " + std::string(format);
	va_start(args, format);
	vLog(log.c_str(), args);
	va_end(args);
}

void dLogger::LogDebug(const std::string& className, const std::string& message) {
	LogDebug(className.c_str(), message.c_str());
}

void dLogger::Flush() {
#ifdef _WIN32
	mFile.flush();
#else
	if (fp != nullptr) {
		std::fflush(fp);
	}
#endif
}
