//server_main.cpp
#include "handlers.h"
#include "key_manager.h"
#include "logger.h"
#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem> // Include this header

int main() {
    initializeLogFiles();
    KeyManager km;

    std::filesystem::path certPath = std::filesystem::current_path() / "certs/myapp-localhost.crt";
    std::filesystem::path keyPath = std::filesystem::current_path() / "certs/myapp-localhost.key";

    httplib::SSLServer svr(certPath.string().c_str(), keyPath.string().c_str());

    // Register handlers (ensure you add all necessary handlers here)
    startKMSServer(km);

    std::cout << "Server started at https://localhost:8080" << std::endl;
    logMessage("Server started at https://localhost:8080", serverLogFile);
    svr.listen("0.0.0.0", 8080);

    return 0;
}

