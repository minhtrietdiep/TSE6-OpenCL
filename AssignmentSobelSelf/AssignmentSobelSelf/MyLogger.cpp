#include <Windows.h>
#include <fstream>
#include <iomanip>

#include "MyLogger.hpp"

MyLogger::MyLogger() {}

void MyLogger::Clear() const {
	std::ofstream logFile;
	logFile.open(file, std::ofstream::out | std::ofstream::trunc);
	logFile.close();
}

void MyLogger::Write(const std::string& text) const {
	std::ofstream logFile(file, std::ios_base::out | std::ios_base::app);
	SYSTEMTIME currTimeLog;
	GetLocalTime(&currTimeLog);
	logFile << "[" <<
	           std::setw(2) << std::setfill('0') << currTimeLog.wHour << ":" <<
	           std::setw(2) << std::setfill('0') << currTimeLog.wMinute << ":" <<
	           std::setw(2) << std::setfill('0') << currTimeLog.wSecond << "." <<
	           std::setw(3) << std::setfill('0') << currTimeLog.wMilliseconds << "] " <<
	           text << "\n";
}

void MyLogger::SetFile(const std::string &fileName) {
	file = fileName;
}
