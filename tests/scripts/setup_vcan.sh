#!/bin/sh
set -eu

INTERFACE="${1:-vcan0}"

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

if ip link show "$INTERFACE" >/dev/null 2>&1; then
    $SUDO ip link set up "$INTERFACE"
    exit 0
fi

$SUDO modprobe vcan
$SUDO ip link add dev "$INTERFACE" type vcan
$SUDO ip link set up "$INTERFACE"
