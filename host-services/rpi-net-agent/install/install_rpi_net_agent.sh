#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="/opt/unitlab/rpi-net-agent"
ENV_DIR="/etc/unitlab"
ENV_FILE="${ENV_DIR}/rpi-net-agent.env"
UNIT_FILE="unitlab-rpi-net-agent.service"

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root (sudo)." >&2
  exit 1
fi

echo "[1/7] Installing host dependencies..."
apt-get update
apt-get install -y python3-venv python3-pip network-manager

echo "[2/7] Preparing directories..."
mkdir -p "${DEST_DIR}" "${ENV_DIR}"

echo "[3/7] Copying agent files..."
rsync -a --delete "${SRC_DIR}/" "${DEST_DIR}/" \
  --exclude '.venv' \
  --exclude '__pycache__' \
  --exclude '*.pyc'

echo "[4/7] Creating virtualenv..."
python3 -m venv "${DEST_DIR}/.venv"
"${DEST_DIR}/.venv/bin/pip" install --upgrade pip
"${DEST_DIR}/.venv/bin/pip" install -e "${DEST_DIR}"

echo "[5/7] Writing env file (if missing)..."
if [[ ! -f "${ENV_FILE}" ]]; then
  cat > "${ENV_FILE}" <<'EOF'
# Redis exposed to host (adjust if your docker compose uses a different port or host)
UNITLAB_NET_AGENT_REDIS_URL=redis://127.0.0.1:6379/0
UNITLAB_NET_AGENT_WIFI_IFACE=wlan0
UNITLAB_NET_AGENT_AP_IP_CIDR=10.42.0.1/24
UNITLAB_NET_AGENT_AP_SSID_PREFIX=[unitlab]-core
UNITLAB_NET_AGENT_AP_PASSWORD_PREFIX=pwd!
UNITLAB_NET_AGENT_LOG_LEVEL=INFO
EOF
fi

echo "[6/7] Installing systemd unit..."
install -m 0644 "${DEST_DIR}/systemd/${UNIT_FILE}" "/etc/systemd/system/${UNIT_FILE}"
systemctl daemon-reload
systemctl enable "${UNIT_FILE}"

echo "[7/7] Starting service..."
systemctl restart "${UNIT_FILE}"
systemctl --no-pager --full status "${UNIT_FILE}" || true

echo "Done."
