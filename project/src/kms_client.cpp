//kms_client.cpp
#include "kms_client.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>

void KMSClient::generateKey() {
    httplib::SSLClient cli("localhost", 8080);
    cli.enable_server_certificate_verification(false);
    auto res = cli.Post("/generate-key");
    if (res && res->status == 200) {
        std::cout << "Key generated: " << res->body << std::endl;
    } else {
        throw std::runtime_error("Error generating key: " + (res ? res->body : "Unknown error"));
    }
}

void KMSClient::storeKey(const std::string& key_id, const std::vector<uint8_t>& key) {
    httplib::SSLClient cli("localhost", 8080);
    cli.enable_server_certificate_verification(false);
    nlohmann::json json = { {"key_id", key_id}, {"key", key} };
    auto res = cli.Post("/store-key", json.dump(), "application/json");
    if (!(res && res->status == 200)) {
        throw std::runtime_error("Error storing key: " + (res ? res->body : "Unknown error"));
    }
}

void KMSClient::rotateKey() {
    httplib::SSLClient cli("localhost", 8080);
    cli.enable_server_certificate_verification(false);
    auto res = cli.Post("/rotate-key");
    if (res && res->status == 200) {
        std::cout << "Key rotated: " << res->body << std::endl;
    } else {
        throw std::runtime_error("Error rotating key: " + (res ? res->body : "Unknown error"));
    }
}

std::vector<uint8_t> KMSClient::fetchKey(const std::string& key_id) {
    httplib::SSLClient cli("localhost", 8080);
    cli.enable_server_certificate_verification(false);
    auto res = cli.Get(("/fetch-key/" + key_id).c_str());
    if (res && res->status == 200) {
        auto json = nlohmann::json::parse(res->body);
        return json.at("key").get<std::vector<uint8_t>>();
    } else {
        throw std::runtime_error("Error fetching key: " + (res ? res->body : "Unknown error"));
    }
}

void KMSClient::deleteKey(const std::string& key_id) {
    httplib::SSLClient cli("localhost", 8080);
    cli.enable_server_certificate_verification(false);
    auto res = cli.Post(("/delete-key/" + key_id).c_str());
    if (res && res->status == 200) {
        std::cout << "Key deleted: " << res->body << std::endl;
    } else {
        throw std::runtime_error("Error deleting key: " + (res ? res->body : "Unknown error"));
    }
}

void KMSClient::generateCert() {
    httplib::SSLClient cli("localhost", 8080);
    cli.enable_server_certificate_verification(false);
    auto res = cli.Post("/generate-cert");
    if (res && res->status == 200) {
        std::cout << "Certificate generated: " << res->body << std::endl;
    } else {
        throw std::runtime_error("Error generating certificate: " + (res ? res->body : "Unknown error"));
    }
}

