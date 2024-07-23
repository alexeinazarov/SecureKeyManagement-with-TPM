//utils.cpp
#include "utils.h"
#include "logger.h"
#include <tss2/tss2_esys.h>
#include <iostream>
#include <vector>
#include <cstring>

std::vector<uint8_t> tpm_hash(const std::string& data) {
    ESYS_CONTEXT* esys_context;
    TSS2_RC rc = Esys_Initialize(&esys_context, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        logErrorMessage("Error initializing TPM", serverErrorLogFile);
        throw std::runtime_error("Error initializing TPM");
    }

    TPM2B_MAX_BUFFER buffer = { .size = static_cast<UINT16>(data.size()) };
    std::memcpy(buffer.buffer, data.data(), data.size());

    TPM2B_DIGEST* digest = NULL;
    TPMT_TK_HASHCHECK* validation = NULL;

    rc = Esys_Hash(
        esys_context,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        &buffer,
        TPM2_ALG_SHA256,
        ESYS_TR_RH_NULL,
        &digest,
        &validation
    );

    if (rc != TSS2_RC_SUCCESS) {
        Esys_Finalize(&esys_context);
        logErrorMessage("Error generating hash using TPM", serverErrorLogFile);
        throw std::runtime_error("Error generating hash using TPM");
    }

    std::vector<uint8_t> hash(digest->buffer, digest->buffer + digest->size);

    Esys_Free(digest);
    Esys_Free(validation);
    Esys_Finalize(&esys_context);

    logMessage("TPM hash generated successfully", serverLogFile);

    return hash;
}

std::vector<uint8_t> tpm_encrypt(const std::string& data) {
    ESYS_CONTEXT* esys_context;
    TSS2_RC rc = Esys_Initialize(&esys_context, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        logErrorMessage("Error initializing TPM", serverErrorLogFile);
        throw std::runtime_error("Error initializing TPM");
    }

    TPM2B_SENSITIVE_CREATE inSensitive = {};
    inSensitive.size = sizeof(TPM2B_SENSITIVE_CREATE);
    inSensitive.sensitive.userAuth.size = 0;
    inSensitive.sensitive.data.size = static_cast<UINT16>(data.size());
    std::memcpy(inSensitive.sensitive.data.buffer, data.data(), data.size());

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
        Esys_Finalize(&esys_context);
        logErrorMessage("Error encrypting data using TPM", serverErrorLogFile);
        throw std::runtime_error("Error encrypting data using TPM");
    }

    std::vector<uint8_t> encryptedData(outPrivate->buffer, outPrivate->buffer + outPrivate->size);

    Esys_Free(outPublic);
    Esys_Free(outPrivate);
    Esys_Free(creationData);
    Esys_Free(creationHash);
    Esys_Free(creationTicket);
    Esys_Finalize(&esys_context);

    logMessage("TPM encryption completed successfully", serverLogFile);

    return encryptedData;
}

std::vector<uint8_t> tpm_sign(const std::string& data) {
    ESYS_CONTEXT* esys_context;
    TSS2_RC rc = Esys_Initialize(&esys_context, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        logErrorMessage("Error initializing TPM", serverErrorLogFile);
        throw std::runtime_error("Error initializing TPM");
    }

    TPMT_SIG_SCHEME inScheme = { .scheme = TPM2_ALG_RSASSA, .details = { .rsassa = { .hashAlg = TPM2_ALG_SHA256 } } };

    TPM2B_DIGEST digest = { .size = sizeof(TPM2B_DIGEST) };
    digest.size = static_cast<UINT16>(data.size());
    std::memcpy(digest.buffer, data.data(), data.size());

    TPMT_SIGNATURE* signature = NULL;

    rc = Esys_Sign(
        esys_context,
        ESYS_TR_RH_OWNER,
        ESYS_TR_PASSWORD,
        ESYS_TR_NONE,
        ESYS_TR_NONE,
        &digest,
        &inScheme,
        NULL,
        &signature
    );

    if (rc != TSS2_RC_SUCCESS) {
        Esys_Finalize(&esys_context);
        logErrorMessage("Error signing data using TPM", serverErrorLogFile);
        throw std::runtime_error("Error signing data using TPM");
    }

    std::vector<uint8_t> signedData(signature->signature.rsassa.sig.buffer, signature->signature.rsassa.sig.buffer + signature->signature.rsassa.sig.size);

    Esys_Free(signature);
    Esys_Finalize(&esys_context);

    logMessage("TPM signature generated successfully", serverLogFile);

    return signedData;
}

