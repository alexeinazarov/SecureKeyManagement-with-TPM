//logger.cpp
#include "logger.h"
#include <iostream>
#include <filesystem>

std::ofstream serverLogFile;
std::ofstream serverErrorLogFile;
std::ofstream clientLogFile;
std::ofstream clientErrorLogFile;

void initializeLogFiles() {
    std::filesystem::create_directories("logs");
    serverLogFile.open("logs/kms_server.log", std::ios_base::app);
    serverErrorLogFile.open("logs/kms_server.err.log", std::ios_base::app);
    clientLogFile.open("logs/kms_client.log", std::ios_base::app);
    clientErrorLogFile.open("logs/kms_client.err.log", std::ios_base::app);

    if (!serverLogFile.is_open()) {
        throw std::runtime_error("Unable to open server log file");
    }
    if (!serverErrorLogFile.is_open()) {
        throw std::runtime_error("Unable to open server error log file");
    }
    if (!clientLogFile.is_open()) {
        throw std::runtime_error("Unable to open client log file");
    }
    if (!clientErrorLogFile.is_open()) {
        throw std::runtime_error("Unable to open client error log file");
    }
}

void logMessage(const std::string& message, std::ofstream& logFile) {
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
    std::cout << message << std::endl;
}

void logErrorMessage(const std::string& message, std::ofstream& errorLogFile) {
    if (errorLogFile.is_open()) {
        errorLogFile << message << std::endl;
    }
    std::cerr << message << std::endl;
}

void logTpmError(TSS2_RC rc, const std::string& functionName) {
    std::string errorMessage = functionName + " failed with error code: " + std::to_string(rc);
    logErrorMessage(errorMessage, serverErrorLogFile);
}

void logTpmError(const std::string& message, const std::string& functionName) {
    std::string errorMessage = functionName + " failed: " + message;
    logErrorMessage(errorMessage, serverErrorLogFile);
}

