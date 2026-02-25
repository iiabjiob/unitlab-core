#!/usr/bin/env bash
set -euo pipefail

TIMEZONE="${TIMEZONE:-Etc/UTC}"
RUN_UPGRADE="${RUN_UPGRADE:-1}"
REBOOT_REQUIRED=0

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

detect_cmdline_file() {
  if [[ -f /boot/firmware/cmdline.txt ]]; then
    echo /boot/firmware/cmdline.txt
    return 0
  fi
  if [[ -f /boot/cmdline.txt ]]; then
    echo /boot/cmdline.txt
    return 0
  fi
  return 1
}

ensure_kernel_cgroup_args() {
  local cmdline_file="$1"
  local current
  current="$(tr -d '\n' < "$cmdline_file")"
  local updated="$current"
  local required_args=(
    cgroup_enable=memory
    cgroup_memory=1
  )
  local arg
  for arg in "${required_args[@]}"; do
    if [[ " $updated " != *" $arg "* ]]; then
      updated="$updated $arg"
    fi
  done

  if [[ "$updated" != "$current" ]]; then
    echo "$updated" > "$cmdline_file"
    REBOOT_REQUIRED=1
    echo "[unitlab] Updated kernel args in $cmdline_file"
  else
    echo "[unitlab] Kernel cgroup args already present in $cmdline_file"
  fi
}

echo "[unitlab] Step 1/10: apt update"
apt-get update

if [[ "$RUN_UPGRADE" == "1" ]]; then
  echo "[unitlab] Step 2/10: apt full-upgrade"
  DEBIAN_FRONTEND=noninteractive apt-get full-upgrade -y
else
  echo "[unitlab] Step 2/10: apt upgrade skipped (RUN_UPGRADE=0)"
fi

echo "[unitlab] Step 3/10: timezone -> $TIMEZONE"
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

echo "[unitlab] Step 4/10: ensure chrony"
if ! dpkg -s chrony >/dev/null 2>&1; then
  DEBIAN_FRONTEND=noninteractive apt-get install -y chrony
fi
systemctl enable --now chrony

echo "[unitlab] Step 5/10: ensure docker + compose plugin"
if ! command -v docker >/dev/null 2>&1; then
  curl -fsSL https://get.docker.com | sh
fi
if ! docker compose version >/dev/null 2>&1; then
  DEBIAN_FRONTEND=noninteractive apt-get install -y docker-compose-plugin
fi

echo "[unitlab] Step 6/10: enable docker service"
systemctl enable --now docker

echo "[unitlab] Step 7/10: create /opt/unitlab structure"
mkdir -p /opt/unitlab/releases /opt/unitlab/shared

if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
  usermod -aG docker "$SUDO_USER" || true
  chown -R "$SUDO_USER":"$SUDO_USER" /opt/unitlab || true
fi

echo "[unitlab] Step 8/10: configure docker log rotation"
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

echo "[unitlab] Step 9/10: ensure kernel cgroup args"
if cmdline_file="$(detect_cmdline_file)"; then
  ensure_kernel_cgroup_args "$cmdline_file"
else
  echo "[unitlab] WARN: cmdline.txt not found (/boot/firmware/cmdline.txt or /boot/cmdline.txt); skipping cgroup boot args"
fi

echo "[unitlab] Step 10/10: complete"
echo "[unitlab] Provisioning finished."
echo "[unitlab] NOTE: logout/login required for docker group to apply"
if (( REBOOT_REQUIRED == 1 )); then
  echo "[unitlab] Recommendation: reboot REQUIRED (kernel cmdline updated)."
else
  echo "[unitlab] Recommendation: reboot host now to apply group/session and service state cleanly."
fi
echo "[unitlab] Command: reboot"
