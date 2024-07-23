#!/bin/bash

set -e

# Define working directory for temporary build files
WORKDIR="/tmp/tpm_build"
SWTMP_DIR="/tmp/swtpm"
LIBTPMS_DIR="/tmp/libtpms"

# Ensure script operates from a known directory
cd ~ || exit 1

# Clean up any existing directories to avoid conflicts
sudo rm -rf $WORKDIR
sudo mkdir -p $WORKDIR

# Change to the working directory
cd $WORKDIR || { echo "Failed to change directory to $WORKDIR"; exit 1; }

# Update package lists and install necessary packages
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    libtss2-dev \
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
    netcat \
    apparmor \
    apparmor-utils

# Clean up any existing directories to avoid conflicts
sudo rm -rf $LIBTPMS_DIR $SWTMP_DIR

# Clone and build libtpms
git clone https://github.com/stefanberger/libtpms.git $LIBTPMS_DIR
cd $LIBTPMS_DIR || { echo "Failed to change directory to $LIBTPMS_DIR"; exit 1; }
./autogen.sh --prefix=/usr --with-tpm2 --with-openssl
make
sudo make install

# Set PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# Verify that pkg-config can find libtpms
pkg-config --cflags --libs libtpms

# Clone and build swtpm
git clone https://github.com/stefanberger/swtpm.git $SWTMP_DIR
cd $SWTMP_DIR || { echo "Failed to change directory to $SWTMP_DIR"; exit 1; }
./autogen.sh --prefix=/usr
make
sudo make install

# Clean up build directories
sudo rm -rf $LIBTPMS_DIR $SWTMP_DIR

# Generate self-signed certificates for swtpm
sudo mkdir -p /etc/swtpm/certs
sudo openssl req -new -x509 -days 365 -nodes -out /etc/swtpm/certs/swtpm-localhost.crt -keyout /etc/swtpm/certs/swtpm-localhost.key -subj "/C=US/ST=Denial/L=Springfield/O=Dis/CN=www.example.com"

# Create and configure TPM state
sudo mkdir -p /var/lib/tpm
sudo chmod 700 /var/lib/tpm
sudo chown -R tss:tss /var/lib/tpm

# Ensure necessary directories exist
echo "Creating necessary directories..."
sudo mkdir -p /var/lib/tpm /var/lib/libvirt/swtpm /run/libvirt/qemu/swtpm /var/log/swtpm/libvirt/qemu
sudo chown -R $(whoami) /var/lib/tpm /var/lib/libvirt/swtpm /run/libvirt/qemu/swtpm /var/log/swtpm/libvirt/qemu

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

# Start swtpm manually for testing
echo "Starting SWTPM..."
swtpm socket --tpmstate dir=/var/lib/libvirt/swtpm --tpm2 --server type=tcp,port=2321 --ctrl type=tcp,port=2322 --flags not-need-init,startup-clear &
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
kill $SWTPM_PID

echo "SWTPM setup and verification completed successfully."

