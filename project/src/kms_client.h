//kms_client.h
#ifndef KMS_CLIENT_H
#define KMS_CLIENT_H

#include <string>
#include <vector>

class KMSClient {
public:
    void generateKey();
    void storeKey(const std::string& key_id, const std::vector<uint8_t>& key);
    void rotateKey();
    std::vector<uint8_t> fetchKey(const std::string& key_id);
    void deleteKey(const std::string& key_id);
    void generateCert(); // Add this function
};

#endif // KMS_CLIENT_H

