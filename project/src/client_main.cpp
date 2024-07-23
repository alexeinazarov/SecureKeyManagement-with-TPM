//client_main.cpp
#include "kms_client.h"
#include "logger.h"
#include <iostream>
#include <filesystem>
#include <vector>

void logMessage(const std::string& message) {
    if (clientLogFile.is_open()) {
        clientLogFile << message << std::endl;
    }
    std::cout << message << std::endl;
}

void logErrorMessage(const std::string& message) {
    if (clientErrorLogFile.is_open()) {
        clientErrorLogFile << message << std::endl;
    }
    std::cerr << message << std::endl;
}

std::vector<uint8_t> stringToVector(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::string vectorToString(const std::vector<uint8_t>& vec) {
    return std::string(vec.begin(), vec.end());
}

int main(int argc, char* argv[]) {
    initializeLogFiles();

    if (argc < 2) {
        logMessage("Usage: " + std::string(argv[0]) + " <command> [<args>]");
        return 1;
    }

    std::string command = argv[1];
    KMSClient client;

    try {
        if (command == "generateKey") {
            client.generateKey();
            logMessage("Key generated successfully.");
        } else if (command == "storeKey") {
            if (argc != 4) {
                logMessage("Usage: " + std::string(argv[0]) + " storeKey <key_id> <key_value>");
                return 1;
            }
            std::vector<uint8_t> keyVector = stringToVector(argv[3]);
            client.storeKey(argv[2], keyVector);
            logMessage("Key stored successfully.");
        } else if (command == "fetchKey") {
            if (argc != 3) {
                logMessage("Usage: " + std::string(argv[0]) + " fetchKey <key_id>");
                return 1;
            }
            std::vector<uint8_t> keyVector = client.fetchKey(argv[2]);
            std::string key = vectorToString(keyVector);
            logMessage("Fetched key: " + key);
        } else if (command == "rotateKey") {
            client.rotateKey();
            logMessage("Key rotated successfully.");
        } else if (command == "deleteKey") {
            if (argc != 3) {
                logMessage("Usage: " + std::string(argv[0]) + " deleteKey <key_id>");
                return 1;
            }
            client.deleteKey(argv[2]);
            logMessage("Key deleted successfully.");
        } else if (command == "generateCert") {
            logMessage("Requesting server to generate certificate...");
            client.generateCert();
            logMessage("Certificate generated successfully.");
        } else {
            logMessage("Unknown command: " + command);
            return 1;
        }
    } catch (const std::exception& e) {
        logErrorMessage("Error: " + std::string(e.what()));
        return 1;
    }

    return 0;
}

