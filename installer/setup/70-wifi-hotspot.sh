#!/bin/bash
set -e

INTERFACE="wlan0"
PROFILE_NAME="core-unit-hotspot"
SSID_PREFIX="[unitLab]_core-"
PASSWORD_PREFIX="pwd!"

# DHCP settings
CORE_IP="192.168.10.1"
DHCP_RANGE_START="192.168.10.10"
DHCP_RANGE_END="192.168.10.254"
DHCP_LEASE="12h"

echo "🔧 Starting CoreUnit hotspot setup..."

# Check interface
if ! ip link show "$INTERFACE" &>/dev/null; then
  echo "❌ Interface $INTERFACE not found. Make sure the Wi-Fi module is available."
  exit 1
fi

# Get MAC and derive suffix
MAC=$(cat /sys/class/net/$INTERFACE/address)
SUFFIX=$(echo "$MAC" | awk -F: '{print toupper($5 $6)}')
SSID="${SSID_PREFIX}${SUFFIX}"
PASSWORD="${PASSWORD_PREFIX}${SUFFIX}"

echo "📶 Configuring Hotspot:"
echo "   ➤ SSID:     $SSID"
echo "   ➤ Password: $PASSWORD"
echo "   ➤ Interface: $INTERFACE"
echo "   ➤ Static IP: $CORE_IP"
echo "   ➤ DHCP:     $DHCP_RANGE_START → $DHCP_RANGE_END ($DHCP_LEASE)"
echo

# Remove any existing hotspot profiles
nmcli connection delete Hotspot 2>/dev/null || true
nmcli connection delete "$PROFILE_NAME" 2>/dev/null || true

# Add new AP profile
nmcli connection add type wifi ifname "$INTERFACE" con-name "$PROFILE_NAME" autoconnect yes ssid "$SSID"
nmcli connection modify "$PROFILE_NAME" 802-11-wireless.mode ap
nmcli connection modify "$PROFILE_NAME" 802-11-wireless.band bg
nmcli connection modify "$PROFILE_NAME" 802-11-wireless.channel 6
nmcli connection modify "$PROFILE_NAME" ipv4.method manual ipv4.addresses "$CORE_IP/24"
nmcli connection modify "$PROFILE_NAME" ipv4.never-default true
nmcli connection modify "$PROFILE_NAME" connection.autoconnect yes
nmcli connection modify "$PROFILE_NAME" 802-11-wireless.hidden no
nmcli connection modify "$PROFILE_NAME" wifi-sec.key-mgmt wpa-psk
nmcli connection modify "$PROFILE_NAME" 802-11-wireless-security.proto rsn
nmcli connection modify "$PROFILE_NAME" wifi-sec.psk "$PASSWORD"


# Check if dnsmasq is installed
if ! command -v dnsmasq >/dev/null; then
  echo "📦 Installing dnsmasq..."
  sudo apt update
  sudo apt install -y dnsmasq
fi

# Backup existing dnsmasq.conf and write custom config
echo "⚙️ Writing dnsmasq config..."
sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.backup 2>/dev/null || true

cat <<EOF | sudo tee /etc/dnsmasq.conf >/dev/null
interface=$INTERFACE
listen-address=$CORE_IP
bind-interfaces
no-hosts
no-resolv
domain-needed
bogus-priv
dhcp-range=$DHCP_RANGE_START,$DHCP_RANGE_END,$DHCP_LEASE
EOF

# Restart dnsmasq
echo "🔄 Restarting dnsmasq..."
sudo systemctl restart dnsmasq
sudo systemctl enable dnsmasq

# Final check
echo
echo "✅ CoreUnit Hotspot is READY!"
echo "   ➤ SSID:     $SSID"
echo "   ➤ Password: $PASSWORD"
echo "   ➤ IP:       $CORE_IP"
echo "   ➤ DHCP:     $DHCP_RANGE_START → $DHCP_RANGE_END"

# Bring up the hotspot (assign IP first)
echo "🚀 Bringing up hotspot interface with static IP..."
nmcli connection up "$PROFILE_NAME"