#!/bin/bash

# Enforce strict mode and debug mode
set -euo pipefail
set -x

# Initialize log file
LOGFILE="setup_app.log"
ERROR_LOGFILE="error.log"
sudo touch "$LOGFILE" "$ERROR_LOGFILE"
sudo chown "$(whoami)":"$(whoami)" "$LOGFILE" "$ERROR_LOGFILE"

# Redirect stdout and stderr to the log file
exec > >(tee -i "$LOGFILE") 2>&1

# Function to set up directories
setup_directory() {
    local dir=$1
    echo "Setting up directory: $dir"
    mkdir -p "$dir"
    sudo chown -R "$(whoami)":"$(whoami)" "$dir"
    sudo chmod -R 755 "$dir"
}

# Function to handle errors
error_handler() {
    local err=$?
    echo "Error occurred in script at line: ${BASH_LINENO[0]}"
    exit $err
}

# Trap errors
trap error_handler ERR

# Generate self-signed certificates
generate_certificates() {
    local cert_dir="$1"
    echo "Generating self-signed certificates in $cert_dir..."
    setup_directory "$cert_dir"
    sudo openssl req -new -x509 -days 365 -nodes -out "$cert_dir/myapp-localhost.crt" -keyout "$cert_dir/myapp-localhost.key" -subj "/C=US/ST=Denial/L=Springfield/O=Dis/CN=www.example.com"
}

# Run TPM setup script
install_swtpm() {
    local script_dir=$1
    echo "Running TPM setup script..."
    cd "$script_dir"
    chmod +x setup_tpm_not_set.sh
    if sudo ./setup_tpm_not_set.sh; then
        echo "TPM setup script completed successfully."
    else
        echo "TPM setup script failed."
        exit 1
    fi
}

# Check SWTPM functionality
check_swtpm_functionality() {
    echo "Checking SWTPM functionality..."
    if ! sudo tpm2_getcap properties-fixed ; then
        echo "SWTPM functionality check failed."
        exit 1
    else
        echo "SWTPM functionality check passed."
    fi
}

# Clean directories
clean_directory() {
    local dir=$1
    echo "Cleaning the directory: $dir"
    sudo rm -rf "$dir"
}

# Function to modify Supervisor configuration
modify_supervisor_config() {
    echo "Modifying Supervisor configuration..."
    sudo cp /etc/supervisor/supervisord.conf /etc/supervisor/supervisord.conf.bak
    sudo sed -i 's|file=/var/run/supervisor.sock|file=/tmp/supervisor.sock|' /etc/supervisor/supervisord.conf
    sudo sed -i 's|chmod=0700|chmod=0770|' /etc/supervisor/supervisord.conf
    sudo sed -i '/\[unix_http_server\]/a chown=root:supervisor' /etc/supervisor/supervisord.conf
    sudo sed -i 's|serverurl=unix:///var/run/supervisor.sock|serverurl=unix:///tmp/supervisor.sock|' /etc/supervisor/supervisord.conf
}

# Function to set up user and group for Supervisor
setup_supervisor_user_group() {
    echo "Setting up supervisor user group..."
    sudo groupadd -f supervisor
    sudo usermod -a -G supervisor "$(whoami)"
    newgrp supervisor <<EONG
    echo "Current user added to supervisor group."
EONG
}

# Function to purge Supervisor
purge_supervisor() {
    echo "Purging Supervisor..."
    echo "Stopping all supervisord processes..."
    sudo pkill -f supervisord
    echo "Checking for any other programs using the required port..."
    sudo lsof -i :9001 || true
    sudo apt-get purge -y supervisor
    echo "Removing lingering files and directories..."
    sudo rm -rf /var/log/supervisor
    sudo rm -rf /etc/supervisor
    sudo rm -f /var/run/supervisor.sock
    sudo rm -f /tmp/supervisor.sock
}

# Function to install Supervisor
install_supervisor() {
    echo "Installing Supervisor..."
    sudo apt-get install -y supervisor
}

# Function to handle Supervisor configuration
setup_supervisor() {
    local project_dir=$1
    local log_dir=$2
    local supervisor_conf="$project_dir/supervisord.conf"
    local supervisor_relative_conf="$project_dir/supervisord_relative.conf"
    local temp_dir="$project_dir/temp"

    setup_directory "$temp_dir"

    if [[ -f "$supervisor_relative_conf" ]]; then
        echo "Converting relative paths to absolute paths in supervisord.conf..."
        sed -e "s#command=./build/kms_server#command=$project_dir/build/kms_server#g" \
            -e "s#directory=./build#directory=$project_dir/build#g" \
            -e "s#stdout_logfile=../logs/kms_server.out.log#stdout_logfile=$log_dir/kms_server.out.log#g" \
            -e "s#stderr_logfile=../logs/kms_server.err.log#stderr_logfile=$log_dir/kms_server.err.log#g" \
            -e "s#command=./test_kms.sh#command=$project_dir/test_kms.sh#g" \
            -e "s#directory=.#directory=$project_dir#g" \
            -e "s#stdout_logfile=./logs/kms_client.out.log#stdout_logfile=$log_dir/kms_client.out.log#g" \
            -e "s#stderr_logfile=./logs/kms_client.err.log#stderr_logfile=$log_dir/kms_client.err.log#g" \
            "$supervisor_relative_conf" > "$supervisor_conf"
        echo "Finished converting relative paths to absolute paths in supervisord.conf..."

        sudo cp "$supervisor_conf" "$temp_dir/supervisord.conf"
        sudo chown "$(whoami)":"$(whoami)" "$temp_dir/supervisord.conf"

        python3 "$project_dir/post_process_supervisor_conf.py" "$temp_dir/supervisord.conf"

        sudo mv "$temp_dir/supervisord.conf" /etc/supervisor/conf.d/supervisord_project.conf
        sudo chown root:root /etc/supervisor/conf.d/supervisord_project.conf
        echo 'Supervisor configuration after post-processing:'
        sudo cat /etc/supervisor/conf.d/supervisord_project.conf

        echo "Killing any existing supervisord processes..."
        sudo pkill -f supervisord || true

        echo "Removing stale supervisor sockets..."
        sudo rm -f /var/run/supervisor.sock || true
        sudo rm -f /tmp/supervisor.sock || true

        echo "Starting supervisord with the new configuration..."
        sudo supervisord -c /etc/supervisor/conf.d/supervisord_project.conf

        echo "Supervisor started with the new configuration."
    else
        echo "Relative Supervisor configuration file not found: $supervisor_relative_conf"
        exit 1
    fi
}

# Function to install necessary packages
install_packages() {
    echo "Updating package list and installing required packages..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        libssl-dev \
        openssl \
        wget \
        git \
        automake \
        autoconf \
        libtool \
        pkg-config \
        libtasn1-6-dev \
        libjson-glib-dev \
        expect \
        gawk \
        socat \
        libseccomp-dev \
        gnutls-bin \
        libgnutls28-dev \
        net-tools \
        iproute2 \
        tpm2-tools \
        tpm2-abrmd \
        lsof \
        dos2unix \
        python3
}

# Function to gracefully stop Supervisor and its processes
stop_supervisor() {
    echo "Initiating graceful shutdown of Supervisor and its managed processes..."

    if sudo supervisorctl stop all; then
        echo "Managed processes stopped successfully."
    else
        echo "Encountered an error while stopping managed processes."
        echo "Performing hard kill of all supervisord processes..."
        sudo pkill -f supervisord
        echo "Purging Supervisor to clear lingering issues..."
        purge_supervisor
    fi

    echo "Waiting for processes to complete tasks (optional, adjust sleep time as needed)..."
    sleep 5

    sudo systemctl stop supervisor || echo "Supervisor was not running."

    # Clean up socket files if they exist
    sudo rm -f /var/run/supervisor.sock
    sudo rm -f /tmp/supervisor.sock

    echo "Supervisor and its managed processes have been stopped gracefully."
}

# Handle SIGINT signal for graceful shutdown
trap 'stop_supervisor' SIGINT

# Main function to orchestrate steps
main() {
    install_packages

    local current_dir
    current_dir=$(pwd)
    local project_dir="${current_dir}/project"
    local cert_dir="$project_dir/certs"
    local log_dir="$project_dir/logs"
    local build_dir="$project_dir/build"
    local include_dir="$project_dir/include"
    local json_include_dir="$include_dir/nlohmann"

    echo "Starting comprehensive application setup..."
    setup_directory "$project_dir"

    generate_certificates "$cert_dir"

    clean_directory "$build_dir"

    setup_directory "$build_dir"

    echo "Deleting old log files..."
    setup_directory "$log_dir"
    sudo rm -f "$log_dir"/*.log
    sudo chmod 777 "$log_dir"

    install_swtpm "$project_dir"
    
    # Check SWTPM functionality immediately after installing and mounting SWTPM
    check_swtpm_functionality

    echo "Building the application..."
    if [[ -d "$project_dir" ]]; then
        cd "$project_dir"
    else
        echo "Project directory not found: $project_dir"
        exit 1
    fi

    echo "Downloading httplib.h..."
    setup_directory "$include_dir"
    wget https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h -O "$include_dir/httplib.h"

    echo "Downloading json.hpp..."
    setup_directory "$json_include_dir"
    wget https://github.com/nlohmann/json/releases/download/v3.10.5/json.hpp -O "$json_include_dir/json.hpp"

    echo "Running cmake..."
    cd "$build_dir"
    cmake ..
    echo "Running make..."
    make


    echo "Ensuring test_kms.sh is executable..."
    sudo chmod +x "$project_dir/test_kms.sh"
    
    echo "Fixing line endings in test_kms.sh..."
    sudo dos2unix "$project_dir/test_kms.sh"

    echo "Ensuring test_kms.sh is executable..."
    sudo chmod +x "$project_dir/test_kms.sh"

    # Purge, install, and configure Supervisor
    purge_supervisor
    install_supervisor
    modify_supervisor_config
    setup_supervisor_user_group
    setup_supervisor "$project_dir" "$log_dir"

    echo "Supervisor setup completed."

    # Enable and start Supervisor service
    echo "Enabling and starting Supervisor service..."
    sudo systemctl enable supervisor
    sudo systemctl start supervisor

    # Check Supervisor status
    echo "Checking Supervisor status..."
    sudo supervisorctl status

    # Tail the logs
    echo "Tailing the logs..."
    tail -f "$log_dir/kms_server.out.log" "$log_dir/kms_server.err.log" "$log_dir/kms_client.out.log" "$log_dir/kms_client.err.log" &

    # Run the test script
    cd "$project_dir"
    ./test_kms.sh

    echo "Application setup, execution, and testing completed successfully."

    # Wait for user input to exit and stop Supervisor gracefully
    echo "Press any key to exit and stop Supervisor gracefully..."
    read -n 1 -s
    stop_supervisor
}

main

