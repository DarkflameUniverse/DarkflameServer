#include "dLogger.h"

dLogger::dLogger(const std::string& outpath, bool logToConsole) {
	m_logToConsole = logToConsole;
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

void dLogger::LogBasic(const std::string & message) {
	LogBasic(message.c_str());
}

void dLogger::LogBasic(const char * format, ...) {
#ifdef _WIN32
	time_t t = time(NULL);
	struct tm time;
	localtime_s(&time, &t);

	char timeStr[70];
	
	strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", &time);

	char message[2048];
	va_list args;
	va_start(args, format);
	vsprintf_s(message, format, args);
	va_end(args);

	if (m_logToConsole) std::cout << "[" << timeStr << "] " << message;
	mFile << "[" << timeStr << "] " << message;
#else
	time_t t = time(NULL);
    struct tm * time = localtime(&t);
    char timeStr[70];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", time);
	
	char message[2048];
    va_list args;
    va_start(args, format);
    vsprintf(message, format, args);
    va_end(args);
    
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

void dLogger::Log(const char * className, const char * format, ...) {
#ifdef _WIN32
	time_t t = time(NULL);
	struct tm time;
	localtime_s(&time, &t);

	char timeStr[70];
	strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", &time);

	char message[2048];
	va_list args;
	va_start(args, format);
	vsprintf_s(message, format, args);

	va_end(args);

	if (m_logToConsole) std::cout << "[" << timeStr << "] [" << className << "]: " << message;
	mFile << "[" << timeStr << "] [" << className << "]: " << message;
#else
	time_t t = time(NULL);
    struct tm * time = localtime(&t);
    char timeStr[70];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%y %H:%M:%S", time);
	
	char message[2048];
    va_list args;
    va_start(args, format);
    vsprintf(message, format, args);
    va_end(args);
    
    if (m_logToConsole) {
		fputs("[", stdout);
		fputs(timeStr, stdout);
		fputs("] ", stdout);
		fputs("[", stdout);
		fputs(className, stdout);
		fputs("]: ", stdout);
		fputs(message, stdout);
    }
    
    if (fp != NULL) {
		fputs("[", fp);
		fputs(timeStr, fp);
		fputs("] ", fp);
		fputs("[", fp);
		fputs(className, fp);
		fputs("]: ", fp);
		fputs(message, fp);
    }
#endif
}

void dLogger::Log(const std::string & className, const std::string & message) {
	Log(className.c_str(), message.c_str());
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
