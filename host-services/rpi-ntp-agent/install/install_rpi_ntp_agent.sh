#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="/opt/unitlab/rpi-ntp-agent"
ENV_DIR="/etc/unitlab"
ENV_FILE="${ENV_DIR}/rpi-ntp-agent.env"
UNIT_FILE="unitlab-rpi-ntp-agent.service"
CHRONY_MAIN_CONF="/etc/chrony/chrony.conf"
CHRONY_DROPIN_DIR="/etc/chrony/conf.d"
CHRONY_DROPIN_PATH="${CHRONY_DROPIN_DIR}/unitlab-local-master.conf"

ensure_chrony_confdir_enabled() {
  local include_line="confdir /etc/chrony/conf.d"
  local include_regex='^[[:space:]]*confdir[[:space:]]+/etc/chrony/conf\.d([[:space:]]|$)'

  if [[ ! -f "${CHRONY_MAIN_CONF}" ]]; then
    echo "chrony main config not found: ${CHRONY_MAIN_CONF}" >&2
    exit 1
  fi

  if grep -Eq "${include_regex}" "${CHRONY_MAIN_CONF}"; then
    return 0
  fi

  printf '\n# UnitLab host-service drop-ins\n%s\n' "${include_line}" >> "${CHRONY_MAIN_CONF}"
}

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run as root (sudo)." >&2
  exit 1
fi

echo "[1/7] Installing host dependencies..."
apt-get update
apt-get install -y python3-venv python3-pip chrony

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
UNITLAB_NTP_AGENT_REDIS_URL=redis://127.0.0.1:6379/0
UNITLAB_NTP_AGENT_LOG_LEVEL=INFO
EOF
fi

echo "[6/8] Installing chrony local master config..."
ensure_chrony_confdir_enabled
mkdir -p "${CHRONY_DROPIN_DIR}"
install -m 0644 "${DEST_DIR}/chrony/unitlab-local-master.conf" "${CHRONY_DROPIN_PATH}"

echo "[7/8] Installing systemd unit..."
install -m 0644 "${DEST_DIR}/systemd/${UNIT_FILE}" "/etc/systemd/system/${UNIT_FILE}"
systemctl daemon-reload
systemctl enable chrony
systemctl enable "${UNIT_FILE}"

echo "[8/8] Starting services..."
systemctl restart chrony
systemctl restart "${UNIT_FILE}"
systemctl --no-pager --full status "${UNIT_FILE}" || true

echo "Done."

