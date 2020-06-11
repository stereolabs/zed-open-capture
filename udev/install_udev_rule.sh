#!/bin/bash

SUDO=''
if (( $EUID != 0 )); then
    SUDO='sudo'
fi
$SUDO cp 99-slabs.rules /etc/udev/rules.d/99-slabs.rules

udevadm trigger
