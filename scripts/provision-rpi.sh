#!/usr/bin/env bash
set -euo pipefail

TIMEZONE="${TIMEZONE:-Etc/UTC}"
RUN_UPGRADE="${RUN_UPGRADE:-1}"

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/provision-rpi.sh [--timezone <Area/City>] [--skip-upgrade]

Options:
  --timezone <Area/City>   Target system timezone (default: Etc/UTC)
  --skip-upgrade           Run apt update only (skip full apt upgrade)
  -h, --help               Show this help

Environment overrides:
  TIMEZONE=<Area/City>
  RUN_UPGRADE=0|1
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --timezone)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --timezone requires a value" >&2; usage; exit 1; }
      TIMEZONE="$2"
      shift 2
      ;;
    --skip-upgrade)
      RUN_UPGRADE="0"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "[unitlab] ERROR: unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ "$RUN_UPGRADE" != "0" && "$RUN_UPGRADE" != "1" ]]; then
  echo "[unitlab] ERROR: RUN_UPGRADE must be 0 or 1" >&2
  exit 1
fi

if [[ "$EUID" -ne 0 ]]; then
  echo "[unitlab] ERROR: run as root (use sudo)." >&2
  exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
  echo "[unitlab] ERROR: apt-get not found. This script targets Debian/Raspberry Pi OS." >&2
  exit 1
fi

echo "[unitlab] Step 1/9: apt update"
apt-get update

if [[ "$RUN_UPGRADE" == "1" ]]; then
  echo "[unitlab] Step 2/9: apt full-upgrade"
  DEBIAN_FRONTEND=noninteractive apt-get full-upgrade -y
else
  echo "[unitlab] Step 2/9: apt upgrade skipped (RUN_UPGRADE=0)"
fi

echo "[unitlab] Step 3/9: timezone -> $TIMEZONE"
if [[ ! -f "/usr/share/zoneinfo/$TIMEZONE" ]]; then
  echo "[unitlab] ERROR: invalid timezone: $TIMEZONE" >&2
  exit 1
fi
if command -v timedatectl >/dev/null 2>&1; then
  timedatectl set-timezone "$TIMEZONE"
else
  echo "$TIMEZONE" > /etc/timezone
  ln -sf "/usr/share/zoneinfo/$TIMEZONE" /etc/localtime
fi

echo "[unitlab] Step 4/9: ensure chrony"
if ! dpkg -s chrony >/dev/null 2>&1; then
  DEBIAN_FRONTEND=noninteractive apt-get install -y chrony
fi
systemctl enable --now chrony

echo "[unitlab] Step 5/9: ensure docker + compose plugin"
if ! command -v docker >/dev/null 2>&1; then
  curl -fsSL https://get.docker.com | sh
fi
if ! docker compose version >/dev/null 2>&1; then
  DEBIAN_FRONTEND=noninteractive apt-get install -y docker-compose-plugin
fi

echo "[unitlab] Step 6/9: enable docker service"
systemctl enable --now docker

echo "[unitlab] Step 7/9: create /opt/unitlab structure"
mkdir -p /opt/unitlab/releases /opt/unitlab/shared

if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
  usermod -aG docker "$SUDO_USER" || true
  chown -R "$SUDO_USER":"$SUDO_USER" /opt/unitlab || true
fi

echo "[unitlab] Step 8/9: configure docker log rotation"
mkdir -p /etc/docker
if command -v python3 >/dev/null 2>&1; then
  python3 <<'PY'
import json
from pathlib import Path

p = Path('/etc/docker/daemon.json')
if p.exists():
    try:
        data = json.loads(p.read_text(encoding='utf-8') or '{}')
    except Exception:
        backup = p.with_suffix('.json.bak')
        p.rename(backup)
        data = {}
else:
    data = {}

data['log-driver'] = 'json-file'
opts = data.get('log-opts')
if not isinstance(opts, dict):
    opts = {}
opts['max-size'] = '10m'
opts['max-file'] = '3'
data['log-opts'] = opts

p.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
PY
else
  if [[ -f /etc/docker/daemon.json ]]; then
    cp /etc/docker/daemon.json /etc/docker/daemon.json.bak
  fi
  cat > /etc/docker/daemon.json <<'EOF'
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "10m",
    "max-file": "3"
  }
}
EOF
fi
systemctl restart docker

echo "[unitlab] Step 9/9: complete"
echo "[unitlab] Provisioning finished."
echo "[unitlab] NOTE: logout/login required for docker group to apply"
echo "[unitlab] Recommendation: reboot host now to apply group/session and service state cleanly."
echo "[unitlab] Command: reboot"
