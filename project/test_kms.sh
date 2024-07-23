#!/bin/bash

# Set project directory
PROJECT_DIR="$(dirname "$0")"
BUILD_DIR="$PROJECT_DIR/build"
TPM_LOG="$PROJECT_DIR/logs/tpm_script.log"
ERROR_LOG="$PROJECT_DIR/logs/tpm_errors.log"
PAUSE_DURATION=5
RETRY_LIMIT=3

# Function to log and explain test steps
log_and_explain() {
    local message=$1
    echo -e "\n[INFO] $(date): $message" | tee -a $TPM_LOG
}

# Function to log errors
log_error() {
    local message=$1
    echo -e "\n[ERROR] $(date): $message" | tee -a $ERROR_LOG
}

# Function to execute a command and log its output
exec_command() {
    local cmd=$1
    log_and_explain "Executing: $cmd"
    eval $cmd 2>&1 | tee -a $ERROR_LOG
    local status=${PIPESTATUS[0]}
    if [ $status -ne 0 ]; then
        log_error "Command failed: $cmd"
    fi
    return $status
}

# Function to check TPM state
check_tpm_state() {
    tpm2_getcap properties-variable | grep -q "TPM2_PT_LOCKOUT_COUNTER: 0"
    if [ $? -ne 0 ]; then
        log_error "TPM is in DA lockout mode. Attempting to clear DA lockout."
        exec_command "tpm2_clearlockout" || {
            log_error "Failed to clear DA lockout. Rebooting swtpm."
            restart_swtpm
            exec_command "tpm2_clearlockout" || {
                log_error "Failed to clear DA lockout after reboot. Exiting script."
                exit 1
            }
        }
        log_and_explain "DA lockout cleared successfully."
    else
        log_and_explain "TPM is not in DA lockout mode."
    fi
}

# Function to set TPM permissions
set_tpm_permissions() {
    sudo chmod 666 /dev/tpm0 /dev/tpmrm0 || {
        log_error "Failed to set permissions on TPM device files. Exiting script."
        exit 1
    }
    log_and_explain "Permissions set on TPM device files."
}

# Function to clear TPM
clear_tpm() {
    local retries=0
    while [ $retries -lt $RETRY_LIMIT ]; do
        exec_command "tpm2_clear"
        if [ $? -eq 0 ]; then
            log_and_explain "TPM cleared successfully."
            return 0
        fi
        log_error "Failed to clear TPM. Retrying in $PAUSE_DURATION seconds."
        sleep $PAUSE_DURATION
        retries=$((retries + 1))
    done
    log_error "Failed to clear TPM after $RETRY_LIMIT attempts. Rebooting swtpm."
    restart_swtpm
    exec_command "tpm2_clear" || {
        log_error "Failed to clear TPM after reboot. Exiting script."
        exit 1
    }
}

# Function to restart swtpm
restart_swtpm() {
    log_and_explain "Restarting swtpm emulator."
    sudo pkill -f swtpm || log_error "swtpm process not found"
    sudo swtpm socket --tpmstate dir=/tmp/myvtpm --ctrl type=unixio,path=/tmp/myvtpm/swtpm-sock --log level=20 --daemon
    sleep 5
    sudo tpm2-abrmd --tcti=swtpm: --session --daemon
    sleep 5
}

# Ensure TPM is ready
initialize_tpm() {
    set_tpm_permissions
    check_tpm_state
    clear_tpm
}

# Function to pause between operations and report the pause
pause_between_operations() {
    log_and_explain "Pausing for $PAUSE_DURATION seconds to ensure TPM stability."
    sleep $PAUSE_DURATION
}

# Main script execution starts here

# Initialize TPM
initialize_tpm

# Wait for the server to start
sleep 5

# Test key generation
log_and_explain "Testing key generation (NIST SP 800-57, Section 5.1.1: Key Generation)."
exec_command "$BUILD_DIR/kms_client generateKey" || log_error "Key generation failed."
pause_between_operations

# Test key storage
log_and_explain "Testing key storage (NIST SP 800-57, Section 5.1.2: Key Storage)."
exec_command "$BUILD_DIR/kms_client storeKey test_key_id test_key_value" || log_error "Key storage failed."
pause_between_operations

# Test key fetching
log_and_explain "Testing key fetching (NIST SP 800-57, Section 5.1.3: Key Retrieval)."
exec_command "$BUILD_DIR/kms_client fetchKey test_key_id" || log_error "Key fetching failed."
pause_between_operations

# Test key rotation
log_and_explain "Testing key rotation (NIST SP 800-57, Section 5.3.5: Key Rotation)."
exec_command "$BUILD_DIR/kms_client rotateKey test_key_id" || log_error "Key rotation failed."
pause_between_operations

# Test key deletion
log_and_explain "Testing key deletion (NIST SP 800-57, Section 5.3.9: Key Deletion)."
exec_command "$BUILD_DIR/kms_client deleteKey test_key_id" || log_error "Key deletion failed."
pause_between_operations

log_and_explain "All tests completed."

