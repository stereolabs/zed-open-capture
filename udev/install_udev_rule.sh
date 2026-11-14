#!/bin/bash

# Installs the udev rules required to access the cameras without root privileges.
# It can be started from any folder.

SUDO=''
if (( $EUID != 0 )); then
    SUDO='sudo'
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

$SUDO cp "${SCRIPT_DIR}/99-slabs.rules" /etc/udev/rules.d/99-slabs.rules || exit 1
$SUDO chmod 644 /etc/udev/rules.d/99-slabs.rules

# Apply the rules to the cameras already connected
$SUDO udevadm control --reload-rules
$SUDO udevadm trigger

echo "udev rules installed"
