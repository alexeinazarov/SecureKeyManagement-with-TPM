//handlers.cpp
#include "handlers.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include "logger.h"

#define SSL_CERT_FILE "/home/alexey/Documents/mit/app2/project/certs/myapp-localhost.crt"
#define SSL_KEY_FILE "/home/alexey/Documents/mit/app2/project/certs/myapp-localhost.key"

void generateSelfSignedCertificate() {
    std::string command = "openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout " SSL_KEY_FILE " -out " SSL_CERT_FILE " -subj \"/C=US/ST=Denial/L=Springfield/O=Dis/CN=www.example.com\"";
    int result = std::system(command.c_str());
    if (result != 0) {
        throw std::runtime_error("Failed to generate self-signed certificate");
    }
    logMessage("Self-signed certificate generated successfully.", serverLogFile);
}

void startKMSServer(KeyManager &keyManager) {
    httplib::SSLServer svr(SSL_CERT_FILE, SSL_KEY_FILE);

    svr.Post("/generate-key", [&](const httplib::Request &req, httplib::Response &res) {
        logMessage("Received request to /generate-key", serverLogFile);
        try {
            auto key = keyManager.generateTPMSymmetricKey();
            std::vector<uint8_t> keyVector = key;
            std::string keyId = std::to_string(std::time(nullptr)); // Generate key ID based on the current time
            keyManager.addKey(keyId, keyVector);
            nlohmann::json json = {{"key_id", keyId}, {"key", nlohmann::json::binary_t(keyVector)}};
            res.set_content(json.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(e.what(), "application/json");
            logErrorMessage("Error generating key: " + std::string(e.what()), serverErrorLogFile);
        }
    });

    svr.Post("/store-key", [&](const httplib::Request &req, httplib::Response &res) {
        logMessage("Received request to /store-key", serverLogFile);
        try {
            auto json = nlohmann::json::parse(req.body);
            std::string key_id = json.at("key_id").get<std::string>();
            std::vector<uint8_t> keyVector = json.at("key").get<std::vector<uint8_t>>();
            keyManager.addKey(key_id, keyVector);
            res.set_content("{\"message\": \"Key stored successfully\"}", "application/json");
        } catch (const nlohmann::json::exception &e) {
            res.status = 400; // Bad Request
            res.set_content(e.what(), "application/json");
            logErrorMessage("JSON error storing key: " + std::string(e.what()), serverErrorLogFile);
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(e.what(), "application/json");
            logErrorMessage("Error storing key: " + std::string(e.what()), serverErrorLogFile);
        }
    });

    svr.Post("/rotate-key", [&](const httplib::Request &req, httplib::Response &res) {
        logMessage("Received request to /rotate-key", serverLogFile);
        try {
            keyManager.rotateKeys();
            res.set_content("{\"message\": \"Key rotated successfully\"}", "application/json");
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(e.what(), "application/json");
            logErrorMessage("Error rotating key: " + std::string(e.what()), serverErrorLogFile);
        }
    });

    svr.Get("/fetch-key/(.*)", [&](const httplib::Request &req, httplib::Response &res) {
        logMessage("Received request to /fetch-key", serverLogFile);
        try {
            std::string key_id = req.matches[1];
            auto key = keyManager.getKey(key_id);
            nlohmann::json json = {{"key_id", key_id}, {"key", nlohmann::json::binary_t(key)}};
            res.set_content(json.dump(), "application/json");
        } catch (const std::exception &e) {
            res.status = 404;
            res.set_content(e.what(), "application/json");
            logErrorMessage("Error fetching key with ID: " + std::string(req.matches[1]) + " - " + std::string(e.what()), serverErrorLogFile);
        }
    });

    svr.Post("/delete-key/(.*)", [&](const httplib::Request &req, httplib::Response &res) {
        logMessage("Received request to /delete-key", serverLogFile);
        try {
            std::string key_id = req.matches[1];
            keyManager.deleteKey(key_id);
            res.set_content("{\"message\": \"Key deleted successfully\"}", "application/json");
        } catch (const std::exception &e) {
            res.status = 404;
            res.set_content(e.what(), "application/json");
            logErrorMessage("Error deleting key with ID: " + std::string(req.matches[1]) + " - " + std::string(e.what()), serverErrorLogFile);
        }
    });

    svr.Post("/generate-cert", [&](const httplib::Request &req, httplib::Response &res) {
        logMessage("Received request to /generate-cert", serverLogFile);
        try {
            generateSelfSignedCertificate();
            res.set_content("{\"message\": \"Certificate generated successfully\"}", "application/json");
        } catch (const std::exception &e) {
            res.status = 500;
            res.set_content(e.what(), "application/json");
            logErrorMessage("Error generating certificate: " + std::string(e.what()), serverErrorLogFile);
        }
    });

    svr.listen("0.0.0.0", 8080);
}

