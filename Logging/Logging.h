#ifndef _LOGGING_H_
#define _LOGGING_H_
#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <string>
#include <mutex>
#include <atomic>

#define EXTRATESTING	  1			  // Perform and log verification tests
#define THREADLOGGING	  0			  // Enable loging of thread events (lock and unlock)
#define LOGGING			  1			  // Enable logging
#define LOGDEBUGGING	  0			  // Undefine this if you don't want the log to contain File/Function/Line of caller
#define SHORTFILENAMES	  0			  // Enable short filenames

#if LOGDEBUGGING && SHORTFILENAMES
#define FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define FILE __FILE__
#endif

static const char* LOGPATH = "eventlog.log";
static std::ofstream logOut;

enum LogEventType {
	ERROR,
	WARNING,
	EVENT,
	THREAD
};

static std::string LogEventTypeToString(LogEventType type) {
	switch (type) {
	case ERROR: return "ERROR";
	case WARNING: return "WARNING";
	case EVENT: return "EVENT";
	case THREAD: return "THREAD";
	}
	return "UNKNOWN";
}

#if LOGGING

static std::mutex printLock;
static void logEvent(LogEventType type, std::string msg) {
	struct tm timeinfo;
#if !THREADLOGGING
	if (type == THREAD)
		return;
#endif
	std::lock_guard<std::mutex> lg(printLock);
	std::ostringstream os;
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	localtime_s(&timeinfo, &now_c);
#if _MSC_VER == 1900
	os << std::put_time(&timeinfo, "%F %T") << " : " << LogEventTypeToString(type) << " - " << msg << "\n";
#else
	os << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << " : " << LogEventTypeToString(type) << " - " << msg << "\n";
#endif
	if (!logOut.is_open()) {
		logOut = std::ofstream(LOGPATH, std::fstream::app);
	}
	logOut << os.str();
}
#else
#define logEvent(type, msg)
#endif

#if LOGDEBUGGING && LOGGING
// class to capture the caller and print it.  
class Reporter
{
public:
	Reporter(std::string Caller, std::string File, int Line)
		: caller_(Caller)
		, file_(File)
		, line_(Line)
	{}

	void operator()(LogEventType type, std::string msg) {
		std::ostringstream lineStr;
		lineStr << line_;
		std::string debuggingMsg = "(" + file_ + "::" + caller_ + ":" + lineStr.str() + ")";
		std::ostringstream newMsg;
		newMsg << std::setw(55) << std::left << msg << std::right << debuggingMsg;
		logEvent(type, newMsg.str());
	}
private:
	std::string   caller_;
	std::string   file_;
	int           line_;
};

// remove the symbol for the function, then define a new version that instead
// creates a stack temporary instance of Reporter initialized with the caller
#  undef logEvent
#  define logEvent Reporter(__FUNCTION__,FILE,__LINE__)
#endif

#endif