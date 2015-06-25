#ifndef _LOGGING_H_
#define _LOGGING_H_
#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <string>

#define LOGDEBUGGING // Undefine this if you don't want the log to contain File/Function/Line of caller

#define FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

static const char* LOGPATH = "rs.log";

enum LogEventType {
	ERROR,
	WARNING,
	EVENT
};

static std::string LogEventTypeToString(LogEventType type) {
	switch (type) {
	case ERROR: return "ERROR"; break;
	case WARNING: return "WARNING"; break;
	case EVENT: return "EVENT"; break;
	}
	return "UNKNOWN";
}

static void logEvent(LogEventType type, std::string msg) {
	struct tm timeinfo;
	std::fstream out(LOGPATH, std::fstream::out | std::fstream::app);
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_c = std::chrono::system_clock::to_time_t(now);
	localtime_s(&timeinfo, &now_c);
	out << std::put_time(&timeinfo, "%F %T") << " : " << LogEventTypeToString(type) << " - " << msg << std::endl;
	out.close();
}

#ifdef LOGDEBUGGING
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
		newMsg << std::setw(50) << std::left << msg << std::right << debuggingMsg;
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