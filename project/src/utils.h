//utils.h
#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

std::vector<uint8_t> tpm_hash(const std::string& data);
std::vector<uint8_t> tpm_encrypt(const std::string& data);
std::vector<uint8_t> tpm_sign(const std::string& data);
void secure_erase(std::vector<uint8_t>& data);

#endif // UTILS_H

