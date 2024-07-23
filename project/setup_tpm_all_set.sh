#!/bin/bash

# Function to check if SWTPM is running and kill it
kill_existing_swtpm() {
  local swtpm_pids
  swtpm_pids=$(pgrep swtpm)
  
  if [ -n "$swtpm_pids" ]; then
    echo "SWTPM processes found: $swtpm_pids. Terminating them..."
    sudo kill -9 $swtpm_pids
    sleep 2  # Give some time for the processes to terminate
  else
    echo "No existing SWTPM processes found."
  fi
}

# Function to check if a port is in use and kill the associated process
check_and_kill_port() {
  local port=$1
  local pid
  pid=$(sudo lsof -t -i :"$port")
  if [ -n "$pid" ]; then
    echo "Port $port is in use by PID $pid. Terminating process..."
    sudo kill -9 "$pid"
    sleep 2  # Give some time for the process to terminate
  fi
}

# Ensure necessary directories exist
echo "Creating necessary directories..."
sudo mkdir -p /var/lib/tpm /var/lib/libvirt/swtpm /run/libvirt/qemu/swtpm /var/log/swtpm/libvirt/qemu
sudo chown -R $(whoami) /var/lib/tpm /var/lib/libvirt/swtpm /run/libvirt/qemu/swtpm /var/log/swtpm/libvirt/qemu

# Define the certificate directory
CERT_DIR="${PROJECT_DIR}/certs"
echo "Using certificate directory: $CERT_DIR"
mkdir -p $CERT_DIR

# Configure AppArmor for swtpm
echo "Configuring AppArmor for swtpm..."
sudo tee /etc/apparmor.d/usr.bin.swtpm > /dev/null <<EOL
#include <tunables/global>

profile swtpm /usr/bin/swtpm {
  #include <abstractions/base>
  #include <abstractions/openssl>

  capability chown,
  capability dac_override,
  capability dac_read_search,
  capability fowner,
  capability fsetid,
  capability setgid,
  capability setuid,

  network inet stream,
  network inet6 stream,
  unix (send) type=dgram addr=none peer=(addr=none),
  unix (send, receive) type=stream addr=none peer=(label=libvirt-*),

  /usr/bin/swtpm rm,

  /tmp/** rwk,
  owner @{HOME}/** rwk,
  owner /var/lib/libvirt/swtpm/** rwk,
  /run/libvirt/qemu/swtpm/*.sock rwk,
  owner /var/log/swtpm/libvirt/qemu/*.log rwk,
  owner /run/libvirt/qemu/swtpm/*.pid rwk,
  owner /dev/vtpmx rw,
  /etc/nsswitch.conf r,
  owner /var/lib/swtpm/** rwk,
  owner /run/swtpm/sock rw,
}
EOL

# Reload AppArmor profiles
echo "Reloading AppArmor..."
sudo apparmor_parser -r /etc/apparmor.d/usr.bin.swtpm
sudo systemctl reload apparmor

# Kill any existing SWTPM processes
kill_existing_swtpm

# Define the ports for SWTPM
SERVER_PORT=2321
CTRL_PORT=2322

echo "Checking and killing processes using ports $SERVER_PORT and $CTRL_PORT..."
check_and_kill_port $SERVER_PORT
check_and_kill_port $CTRL_PORT

# Start swtpm manually for testing
SWTPM_CMD="swtpm socket --tpmstate dir=/var/lib/libvirt/swtpm --tpm2 --server type=tcp,port=$SERVER_PORT --ctrl type=tcp,port=$CTRL_PORT --flags not-need-init,startup-clear"
echo "Starting SWTPM with command: $SWTPM_CMD"
$SWTPM_CMD &
SWTPM_PID=$!
sleep 5

# Check if swtpm is running
if ps -p $SWTPM_PID > /dev/null
then
   echo "SWTPM started successfully."
else
   echo "Failed to start SWTPM."
   exit 1
fi

# Verifying TPM functionality
echo "Verifying TPM functionality..."
TPM_VERIFICATION=$(tpm2_getrandom 4 2>&1)
if [[ $TPM_VERIFICATION == *"ERROR"* ]]; then
   echo "TPM verification failed: $TPM_VERIFICATION"
else
   echo "TPM is functioning correctly."
fi

# Cleanup
echo "Cleaning up..."
kill $SWTPM_PID

echo "SWTPM setup and verification completed successfully."

