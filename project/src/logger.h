//logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>
#include <tss2/tss2_rc.h>

extern std::ofstream serverLogFile;
extern std::ofstream serverErrorLogFile;
extern std::ofstream clientLogFile;
extern std::ofstream clientErrorLogFile;

void logMessage(const std::string& message, std::ofstream& logFile);
void logErrorMessage(const std::string& message, std::ofstream& errorLogFile);
void initializeLogFiles();
void logTpmError(TSS2_RC rc, const std::string& functionName);
void logTpmError(const std::string& message, const std::string& functionName);

#endif // LOGGER_H

