// key_manager.h
#ifndef KEY_MANAGER_H
#define KEY_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <ctime>
#include <utility>

class KeyManager {
public:
    KeyManager();

    std::vector<uint8_t> generateTPMSymmetricKey();
    void rotateKeys();
    void addKey(const std::string& key_id, const std::vector<uint8_t>& key);
    std::vector<uint8_t> getKey(const std::string& key_id);
    void deleteKey(const std::string& key_id);

private:
    std::unordered_map<std::string, std::vector<uint8_t>> keys;
    std::mutex keysMutex;

    std::vector<uint8_t> sealKey(const std::vector<uint8_t>& key);
    std::vector<uint8_t> unsealKey(const std::vector<uint8_t>& sealedKey);
    static constexpr int rotationPeriodDays = 30;
};

#endif // KEY_MANAGER_H


