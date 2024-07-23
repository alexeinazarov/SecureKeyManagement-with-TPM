#!/bin/bash

# Enforce strict mode and debug mode
set -euo pipefail
set -x

# Initialize log file
LOGFILE="./project/logs/clear_tpm_lockout.log"
ERROR_LOGFILE="error.log"
sudo touch "$LOGFILE" "$ERROR_LOGFILE"
sudo chown "$(whoami)":"$(whoami)" "$LOGFILE" "$ERROR_LOGFILE"

# Redirect stdout and stderr to the log file
exec > >(tee -i "$LOGFILE") 2>&1

# Function to log and explain test steps
log_and_explain() {
    local message=$1
    echo -e "\n[INFO] $(date): $message"
}

# Function to execute a command and handle errors
execute_command() {
    local cmd=$1
    local retries=3
    local count=0
    local delay=5

    while [ $count -lt $retries ]; do
        log_and_explain "Executing: $cmd"
        if $cmd; then
            return 0
        else
            echo "[ERROR] $(date): Command failed: $cmd"
            count=$((count + 1))
            echo "[INFO] $(date): Retrying in $delay seconds... (Attempt $count/$retries)"
            sleep $delay
        fi
    done

    echo "[ERROR] $(date): Command failed after $retries attempts: $cmd"
    return 1
}

# Ensure TPM device files have correct permissions
log_and_explain "Setting permissions on TPM device files."
sudo chmod 666 /dev/tpm0 /dev/tpmrm0

# Function to restart swtpm
restart_swtpm() {
    log_and_explain "Restarting swtpm emulator."
    sudo pkill swtpm || true
    sleep 5
    sudo mkdir -p /tmp/myvtpm
    sudo swtpm socket --tpmstate dir=/tmp/myvtpm --ctrl type=unixio,path=/tmp/myvtpm/swtpm-sock --log level=20 --daemon
    if [ $? -ne 0 ]; then
        echo "[ERROR] $(date): Failed to start swtpm emulator."
        exit 1
    fi
    sleep 5

    # Verify swtpm is running
    if ! pgrep -f swtpm > /dev/null; then
        echo "[ERROR] $(date): swtpm is not running."
        exit 1
    fi
}

# Function to start tpm2-abrmd
start_tpm2_abrmd() {
    log_and_explain "Starting tpm2-abrmd."
    sudo pkill tpm2-abrmd || true
    sleep 5
    sudo tpm2-abrmd --tcti=swtpm: --allow-root &
    sleep 5

    # Verify tpm2-abrmd is running
    if ! pgrep -f tpm2-abrmd > /dev/null; then
        echo "[ERROR] $(date): Failed to start tpm2-abrmd."
        exit 1
    fi
}

# Function to clear DA lockout
clear_dalockout() {
    if ! execute_command "sudo tpm2_clearlockout"; then
        log_and_explain "Failed to clear DA lockout. Rebooting swtpm."
        restart_swtpm
        start_tpm2_abrmd
        if ! execute_command "sudo tpm2_clearlockout"; then
            log_and_explain "Failed to clear DA lockout after reboot. Exiting script."
            exit 1
        fi
    fi
}

# Function to clear TPM
clear_tpm() {
    if ! execute_command "sudo tpm2_clear"; then
        log_and_explain "Failed to clear TPM. Rebooting swtpm."
        restart_swtpm
        start_tpm2_abrmd
        if ! execute_command "sudo tpm2_clear"; then
            log_and_explain "Failed to clear TPM after reboot. Exiting script."
            exit 1
        fi
    fi
}

# Main function
main() {
    log_and_explain "Starting TPM lockout clearing script."

    # Install tpm2-tools if not already installed
    log_and_explain "Ensuring tpm2-tools is installed."
    sudo apt-get update
    sudo apt-get install -y tpm2-tools tpm2-abrmd swtpm

    # Restart swtpm service
    restart_swtpm
    start_tpm2_abrmd

    # Clear DA lockout
    clear_dalockout

    # Clear TPM
    clear_tpm

    # Check TPM capabilities
    log_and_explain "Checking TPM capabilities."
    tpm2_getcap properties-fixed
    tpm2_getcap properties-variable

    log_and_explain "TPM lockout clearing script completed successfully."
}

main

