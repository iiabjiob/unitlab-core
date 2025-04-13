#!/bin/bash

set -e

INTERFACE="wlan0"
PROFILE_NAME="core-unit-hotspot"
SSID_PREFIX="[unitLab]_core-unit-"
PASSWORD_PREFIX="pwd!"

# Check if interface exists
if ! ip link show "$INTERFACE" &>/dev/null; then
  echo "❌ Interface $INTERFACE not found. Make sure the Wi-Fi module is available."
  exit 1
fi

# Get MAC address and build suffix
MAC=$(cat /sys/class/net/$INTERFACE/address)
SUFFIX=$(echo "$MAC" | awk -F: '{print toupper($5 $6)}')
SSID="${SSID_PREFIX}${SUFFIX}"
PASSWORD="${PASSWORD_PREFIX}${SUFFIX}"

echo "🔧 Creating Wi-Fi hotspot:"
echo "   SSID:     $SSID"
echo "   Password: $PASSWORD"
echo "   Interface: $INTERFACE"
echo

# Remove any previous hotspot connections
nmcli connection delete Hotspot 2>/dev/null || true
nmcli connection delete "$PROFILE_NAME" 2>/dev/null || true

# Create new Wi-Fi connection in AP mode
nmcli connection add type wifi ifname "$INTERFACE" con-name "$PROFILE_NAME" autoconnect yes ssid "$SSID"

# Configure AP parameters
nmcli connection modify "$PROFILE_NAME" 802-11-wireless.mode ap
nmcli connection modify "$PROFILE_NAME" 802-11-wireless.band bg
nmcli connection modify "$PROFILE_NAME" ipv4.method shared
nmcli connection modify "$PROFILE_NAME" wifi-sec.key-mgmt wpa-psk
nmcli connection modify "$PROFILE_NAME" wifi-sec.psk "$PASSWORD"

# Bring connection up
nmcli connection up "$PROFILE_NAME"

echo
echo "✅ Hotspot profile '$PROFILE_NAME' created and activated."
