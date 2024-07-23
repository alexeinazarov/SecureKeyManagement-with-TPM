// key_manager.cpp
#include "key_manager.h"
#include "utils.h"
#include "logger.h"
#include <tss2/tss2_esys.h>
#include <iostream>
#include <ctime>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>

KeyManager::KeyManager() {}

std::vector<uint8_t> KeyManager::generateTPMSymmetricKey() {
    ESYS_CONTEXT* esys_context;
    TSS2_RC rc = Esys_Initialize(&esys_context, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_Initialize");
        throw std::runtime_error("Error initializing TPM");
    }

    TPM2B_DIGEST* randomBytes = NULL;
    rc = Esys_GetRandom(esys_context, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, 32, &randomBytes);
    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_GetRandom");
        Esys_Finalize(&esys_context);
        throw std::runtime_error("Error generating random bytes using TPM");
    }

    std::vector<uint8_t> key(randomBytes->buffer, randomBytes->buffer + randomBytes->size);
    Esys_Free(randomBytes);
    Esys_Finalize(&esys_context);

    logMessage("TPM symmetric key generated successfully", serverLogFile);
    return key;
}

void KeyManager::rotateKeys() {
    std::lock_guard<std::mutex> lock(keysMutex);
    auto now = std::time(nullptr);

    for (auto it = keys.begin(); it != keys.end();) {
        if (difftime(now, std::stol(it->first)) > rotationPeriodDays * 24 * 3600) {
            it = keys.erase(it);
        } else {
            ++it;
        }
    }

    try {
        auto newKey = generateTPMSymmetricKey();
        auto sealedKey = sealKey(newKey);
        addKey(std::to_string(std::time(nullptr)), sealedKey);
    } catch (const std::exception& e) {
        logErrorMessage("Error rotating key: " + std::string(e.what()), serverErrorLogFile);
    }
}

void KeyManager::addKey(const std::string& key_id, const std::vector<uint8_t>& key) {
    std::lock_guard<std::mutex> lock(keysMutex);
    keys[key_id] = key;
}

std::vector<uint8_t> KeyManager::getKey(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(keysMutex);
    if (keys.find(key_id) != keys.end()) {
        try {
            return unsealKey(keys[key_id]);
        } catch (const std::exception& e) {
            logErrorMessage("Error unsealing key: " + std::string(e.what()), serverErrorLogFile);
        }
    }
    throw std::runtime_error("Key not found");
}

void KeyManager::deleteKey(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(keysMutex);
    if (keys.erase(key_id) == 0) {
        throw std::runtime_error("Key not found for deletion");
    }
}

std::vector<uint8_t> KeyManager::sealKey(const std::vector<uint8_t>& key) {
    ESYS_CONTEXT* esys_context;
    TSS2_RC rc = Esys_Initialize(&esys_context, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_Initialize");
        throw std::runtime_error("Error initializing TPM");
    }

    TPM2B_SENSITIVE_CREATE inSensitive = {};
    inSensitive.size = sizeof(TPM2B_SENSITIVE_CREATE);
    inSensitive.sensitive.userAuth.size = 0;
    inSensitive.sensitive.data.size = static_cast<UINT16>(key.size());
    memcpy(inSensitive.sensitive.data.buffer, key.data(), key.size());

    TPM2B_PUBLIC inPublic = {};
    inPublic.size = sizeof(TPM2B_PUBLIC);
    inPublic.publicArea.type = TPM2_ALG_SYMCIPHER;
    inPublic.publicArea.nameAlg = TPM2_ALG_SHA256;
    inPublic.publicArea.objectAttributes = (TPMA_OBJECT_USERWITHAUTH | TPMA_OBJECT_RESTRICTED |
                                             TPMA_OBJECT_DECRYPT | TPMA_OBJECT_FIXEDTPM |
                                             TPMA_OBJECT_FIXEDPARENT | TPMA_OBJECT_SENSITIVEDATAORIGIN);
    inPublic.publicArea.parameters.symDetail.sym.algorithm = TPM2_ALG_AES;
    inPublic.publicArea.parameters.symDetail.sym.keyBits.aes = 128;
    inPublic.publicArea.parameters.symDetail.sym.mode.aes = TPM2_ALG_CFB;
    inPublic.publicArea.unique.sym.size = 0;

    TPM2B_DATA outsideInfo = {};
    TPML_PCR_SELECTION creationPCR = {};
    TPM2B_PRIVATE* outPrivate = NULL;
    TPM2B_PUBLIC* outPublic = NULL;
    TPM2B_CREATION_DATA* creationData = NULL;
    TPM2B_DIGEST* creationHash = NULL;
    TPMT_TK_CREATION* creationTicket = NULL;

    rc = Esys_Create(
        esys_context,
        ESYS_TR_RH_OWNER,
        ESYS_TR_PASSWORD,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        &inSensitive,
        &inPublic,
        &outsideInfo,
        &creationPCR,
        &outPrivate,
        &outPublic,
        &creationData,
        &creationHash,
        &creationTicket
    );

    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_Create");
        Esys_Finalize(&esys_context);
        throw std::runtime_error("Error sealing key using TPM");
    }

    std::vector<uint8_t> sealedKey(outPrivate->buffer, outPrivate->buffer + outPrivate->size);

    Esys_Free(outPrivate);
    Esys_Free(outPublic);
    Esys_Free(creationData);
    Esys_Free(creationHash);
    Esys_Free(creationTicket);
    Esys_Finalize(&esys_context);

    logMessage("TPM key sealed successfully", serverLogFile);
    return sealedKey;
}

std::vector<uint8_t> KeyManager::unsealKey(const std::vector<uint8_t>& sealedKey) {
    ESYS_CONTEXT* esys_context;
    TSS2_RC rc = Esys_Initialize(&esys_context, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_Initialize");
        throw std::runtime_error("Error initializing TPM");
    }

    TPM2B_PRIVATE inPrivate = { .size = static_cast<UINT16>(sealedKey.size()) };
    memcpy(inPrivate.buffer, sealedKey.data(), sealedKey.size());

    TPM2B_PUBLIC* inPublic = NULL;
    ESYS_TR objectHandle;
    rc = Esys_Load(
        esys_context,
        ESYS_TR_RH_OWNER,
        ESYS_TR_PASSWORD,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        &inPrivate,
        inPublic,
        &objectHandle
    );

    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_Load");
        Esys_Finalize(&esys_context);
        throw std::runtime_error("Error unsealing key using TPM");
    }

    TPM2B_SENSITIVE_DATA* outData;
    rc = Esys_Unseal(
        esys_context,
        objectHandle,
        ESYS_TR_PASSWORD,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        &outData
    );

    if (rc != TSS2_RC_SUCCESS) {
        logTpmError(rc, "Esys_Unseal");
        Esys_Finalize(&esys_context);
        throw std::runtime_error("Error unsealing key using TPM");
    }

    std::vector<uint8_t> key(outData->buffer, outData->buffer + outData->size);
    Esys_Free(outData);
    Esys_Finalize(&esys_context);

    logMessage("TPM key unsealed successfully", serverLogFile);
    return key;
}

