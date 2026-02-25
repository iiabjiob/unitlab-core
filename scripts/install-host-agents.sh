#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST_AGENTS_DIR="${HOST_AGENTS_DIR:-$ROOT_DIR/host-services}"
if [[ ! -d "$HOST_AGENTS_DIR" && -d "$ROOT_DIR/host-agents" ]]; then
  HOST_AGENTS_DIR="$ROOT_DIR/host-agents"
fi
BUNDLE_ROOT="${BUNDLE_ROOT:-$ROOT_DIR}"
WHEELS_DIR="${WHEELS_DIR:-$BUNDLE_ROOT/wheels}"

INSTALL_ROOT="/opt/unitlab"
ENV_ROOT="/etc/unitlab"
SKIP_APT=0
DRY_RUN=0
NO_RESTART=0
SKIP_PIP_UPGRADE=0
ONLY_RAW=""

readonly KNOWN_AGENTS=(
  rpi-net-agent
  rpi-ntp-agent
  rpi-core-diag-agent
  rpi-provision-agent
)

declare -A AGENT_UNITS=(
  [rpi-net-agent]="unitlab-rpi-net-agent.service"
  [rpi-ntp-agent]="unitlab-rpi-ntp-agent.service"
  [rpi-core-diag-agent]="unitlab-rpi-core-diag-agent.service"
  [rpi-provision-agent]="unitlab-rpi-provision-agent.service"
)

declare -A AGENT_MODULES=(
  [rpi-net-agent]="unitlab_rpi_net_agent"
  [rpi-ntp-agent]="unitlab_rpi_ntp_agent"
  [rpi-core-diag-agent]="unitlab_rpi_core_diag_agent"
  [rpi-provision-agent]="unitlab_rpi_provision_agent"
)

declare -A AGENT_WHEEL_GLOBS=(
  [rpi-net-agent]="unitlab_rpi_net_agent-*.whl"
  [rpi-ntp-agent]="unitlab_rpi_ntp_agent-*.whl"
  [rpi-core-diag-agent]="unitlab_rpi_core_diag_agent-*.whl"
  [rpi-provision-agent]="unitlab_rpi_provision_agent-*.whl"
)

declare -A AGENT_APT_DEPS=(
  [rpi-net-agent]="network-manager"
  [rpi-ntp-agent]="chrony"
  [rpi-core-diag-agent]=""
  [rpi-provision-agent]="curl"
)

readonly COMMON_APT_DEPS=(python3-venv python3-pip)

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/install-host-agents.sh [options]

Options:
  --only <agent1,agent2>   Install subset only (default: all known agents)
  --wheels-dir <path>      Wheelhouse directory (default: <bundle>/wheels)
  --skip-apt               Skip apt update/install steps
  --skip-pip-upgrade       Skip pip self-upgrade inside agent virtualenv
  --dry-run                Print actions without changing system
  --no-restart             Install/update but do not restart services
  -h, --help               Show this help

Environment overrides:
  HOST_AGENTS_DIR
  BUNDLE_ROOT
  WHEELS_DIR
EOF
}

log() { echo "[unitlab][host-agents] $*"; }
err() { echo "[unitlab][host-agents] ERROR: $*" >&2; }

run() {
  if (( DRY_RUN == 1 )); then
    echo "[DRY-RUN] $*"
    return 0
  fi
  "$@"
}

run_shell() {
  local command="$1"
  if (( DRY_RUN == 1 )); then
    echo "[DRY-RUN] $command"
    return 0
  fi
  bash -lc "$command"
}

contains() {
  local needle="$1"; shift
  local item
  for item in "$@"; do
    [[ "$item" == "$needle" ]] && return 0
  done
  return 1
}

write_unit_override() {
  local agent="$1"
  local unit_name="$2"
  local dropin_dir="/etc/systemd/system/${unit_name}.d"
  local override_path="$dropin_dir/override.conf"
  local expected_env="EnvironmentFile=-$ENV_ROOT/$agent.env"
  local expected_exec="ExecStart=$INSTALL_ROOT/$agent/.venv/bin/python -m ${AGENT_MODULES[$agent]}"

  log "[agent:$agent] writing drop-in override for $unit_name"
  run mkdir -p "$dropin_dir"
  if (( DRY_RUN == 1 )); then
    echo "[DRY-RUN] write $override_path with EnvironmentFile/ExecStart override"
    return 0
  fi

  cat > "$override_path" <<EOF
[Service]
$expected_env
ExecStart=
$expected_exec
EOF
}

ensure_unit_runtime_bindings() {
  local agent="$1"
  local unit_name="$2"
  local unit_path="/etc/systemd/system/$unit_name"
  local expected_env_regex="^EnvironmentFile=-?$ENV_ROOT/$agent\\.env$"
  local expected_exec_regex="^ExecStart=$INSTALL_ROOT/$agent/\\.venv/bin/python -m ${AGENT_MODULES[$agent]}$"

  if (( DRY_RUN == 1 )); then
    log "[agent:$agent] runtime binding check skipped in dry-run"
    return 0
  fi

  if [[ ! -f "$unit_path" ]]; then
    err "installed unit not found: $unit_path"
    exit 1
  fi

  if grep -Eq "$expected_env_regex" "$unit_path" && grep -Eq "$expected_exec_regex" "$unit_path"; then
    log "[agent:$agent] unit already contains expected EnvironmentFile and ExecStart"
    return 0
  fi

  write_unit_override "$agent" "$unit_name"
}

parse_only() {
  local csv="$1"
  local old_ifs="$IFS"
  IFS=',' read -r -a parsed <<< "$csv"
  IFS="$old_ifs"

  SELECTED_AGENTS=()
  local raw
  for raw in "${parsed[@]}"; do
    local agent="${raw//[[:space:]]/}"
    [[ -n "$agent" ]] || continue
    if ! contains "$agent" "${KNOWN_AGENTS[@]}"; then
      err "unknown agent in --only: $agent"
      usage
      exit 1
    fi
    if ! contains "$agent" "${SELECTED_AGENTS[@]:-}"; then
      SELECTED_AGENTS+=("$agent")
    fi
  done

  if [[ ${#SELECTED_AGENTS[@]} -eq 0 ]]; then
    err "--only resolved to empty agent list"
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --only)
      [[ $# -ge 2 ]] || { err "--only requires a value"; usage; exit 1; }
      ONLY_RAW="$2"
      shift 2
      ;;
    --wheels-dir)
      [[ $# -ge 2 ]] || { err "--wheels-dir requires a value"; usage; exit 1; }
      WHEELS_DIR="$2"
      shift 2
      ;;
    --skip-apt)
      SKIP_APT=1
      shift
      ;;
    --skip-pip-upgrade)
      SKIP_PIP_UPGRADE=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --no-restart)
      NO_RESTART=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      err "unknown argument: $1"
      usage
      exit 1
      ;;
  esac
done

if [[ "$EUID" -ne 0 ]]; then
  err "run as root (sudo)"
  exit 1
fi

if [[ ! -d "$HOST_AGENTS_DIR" ]]; then
  err "host agents directory not found: $HOST_AGENTS_DIR"
  exit 1
fi

if [[ -n "$ONLY_RAW" ]]; then
  declare -a SELECTED_AGENTS=()
  parse_only "$ONLY_RAW"
else
  declare -a SELECTED_AGENTS=("${KNOWN_AGENTS[@]}")
fi

log "Selected agents: ${SELECTED_AGENTS[*]}"
log "Source root: $HOST_AGENTS_DIR"
log "Wheelhouse: $WHEELS_DIR"

if (( SKIP_APT == 1 )); then
  log "--skip-apt enabled, pip self-upgrade will be skipped"
fi

if (( SKIP_APT == 0 )); then
  if ! command -v apt-get >/dev/null 2>&1; then
    err "apt-get not found (use --skip-apt only if dependencies are already installed)"
    exit 1
  fi
fi

mkdir -p "$INSTALL_ROOT" "$ENV_ROOT"

if (( SKIP_APT == 0 )); then
  log "[1/6] Installing apt dependencies"
  declare -A DEP_SET=()
  local_dep=""
  for dep in "${COMMON_APT_DEPS[@]}"; do DEP_SET["$dep"]=1; done
  for agent in "${SELECTED_AGENTS[@]}"; do
    local_dep="${AGENT_APT_DEPS[$agent]}"
    if [[ -n "$local_dep" ]]; then
      for dep in $local_dep; do DEP_SET["$dep"]=1; done
    fi
  done

  declare -a ALL_DEPS=()
  for dep in "${!DEP_SET[@]}"; do ALL_DEPS+=("$dep"); done
  if [[ ${#ALL_DEPS[@]} -gt 0 ]]; then
    IFS=$'\n' ALL_DEPS=($(printf '%s\n' "${ALL_DEPS[@]}" | sort -u))
    unset IFS
    run apt-get update
    run apt-get install -y "${ALL_DEPS[@]}"
  fi
else
  log "[1/6] Skipping apt dependencies (--skip-apt)"
fi

log "[2/6] Validating agent paths and wheelhouse"
[[ -d "$WHEELS_DIR" ]] || { err "wheelhouse directory missing: $WHEELS_DIR"; exit 1; }
for agent in "${SELECTED_AGENTS[@]}"; do
  src_dir="$HOST_AGENTS_DIR/$agent"
  unit_name="${AGENT_UNITS[$agent]}"
  unit_src="$src_dir/systemd/$unit_name"
  wheel_glob="${AGENT_WHEEL_GLOBS[$agent]}"

  [[ -d "$src_dir" ]] || { err "agent source dir missing: $src_dir"; exit 1; }
  [[ -f "$unit_src" ]] || { err "systemd unit missing: $unit_src"; exit 1; }
  if ! compgen -G "$WHEELS_DIR/$wheel_glob" >/dev/null; then
    err "wheel not found for $agent (expected pattern: $WHEELS_DIR/$wheel_glob)"
    exit 1
  fi
done

if ! compgen -G "$WHEELS_DIR/redis-*.whl" >/dev/null; then
  err "redis dependency wheel missing (expected: $WHEELS_DIR/redis-*.whl)"
  exit 1
fi

log "[3/6] Creating virtualenvs and installing wheels"
for agent in "${SELECTED_AGENTS[@]}"; do
  dest_dir="$INSTALL_ROOT/$agent"
  wheel_glob="${AGENT_WHEEL_GLOBS[$agent]}"
  wheel_path="$(ls -1 "$WHEELS_DIR"/$wheel_glob | sort -V | tail -n1)"

  log "[agent:$agent] ensure runtime dir -> $dest_dir"
  run mkdir -p "$dest_dir"

  log "[agent:$agent] venv + wheel install"
  run python3 -m venv "$dest_dir/.venv"
  if (( SKIP_PIP_UPGRADE == 0 && SKIP_APT == 0 )); then
    if ! run "$dest_dir/.venv/bin/pip" install --upgrade pip; then
      log "[agent:$agent] pip self-upgrade failed, continuing"
    fi
  else
    log "[agent:$agent] pip self-upgrade skipped"
  fi
  run "$dest_dir/.venv/bin/pip" install --no-cache-dir --no-index --find-links "$WHEELS_DIR" --upgrade redis
  run "$dest_dir/.venv/bin/pip" install --no-cache-dir --no-index --find-links "$WHEELS_DIR" --upgrade "$wheel_path"
done

log "[4/6] Ensuring env files"
for agent in "${SELECTED_AGENTS[@]}"; do
  src_dir="$HOST_AGENTS_DIR/$agent"
  env_path="$ENV_ROOT/$agent.env"
  env_example="$src_dir/.env.example"

  if [[ -f "$env_path" ]]; then
    log "[agent:$agent] env exists, preserving: $env_path"
    continue
  fi

  if [[ -f "$env_example" ]]; then
    log "[agent:$agent] creating env from example"
    run install -m 0640 "$env_example" "$env_path"
  else
    log "[agent:$agent] creating placeholder env"
    if (( DRY_RUN == 1 )); then
      echo "[DRY-RUN] write placeholder env -> $env_path"
    else
      cat > "$env_path" <<EOF
# $agent runtime configuration
# Populate required variables for this agent before production use.
# File is preserved across installer reruns.
EOF
      chmod 0640 "$env_path"
    fi
  fi
done

log "[5/6] Installing systemd units"
for agent in "${SELECTED_AGENTS[@]}"; do
  unit_name="${AGENT_UNITS[$agent]}"
  unit_src="$HOST_AGENTS_DIR/$agent/systemd/$unit_name"
  unit_dst="/etc/systemd/system/$unit_name"
  log "[agent:$agent] install unit $unit_name"
  run install -m 0644 "$unit_src" "$unit_dst"
  ensure_unit_runtime_bindings "$agent" "$unit_name"
done
run systemctl daemon-reload
if command -v systemd-analyze >/dev/null 2>&1; then
  for agent in "${SELECTED_AGENTS[@]}"; do
    unit_name="${AGENT_UNITS[$agent]}"
    run_shell "systemd-analyze verify /etc/systemd/system/$unit_name || true"
  done
fi

if (( NO_RESTART == 1 )); then
  log "[6/6] Enabling services (no restart mode)"
else
  log "[6/6] Enabling and restarting services"
fi

net_agent_selected=0
if contains "rpi-net-agent" "${SELECTED_AGENTS[@]}"; then
  net_agent_selected=1
fi

for agent in "${SELECTED_AGENTS[@]}"; do
  if [[ "$agent" == "rpi-net-agent" ]]; then
    continue
  fi
  unit_name="${AGENT_UNITS[$agent]}"
  log "[agent:$agent] enable $unit_name"
  run systemctl enable "$unit_name"
  if (( NO_RESTART == 0 )); then
    log "[agent:$agent] restart $unit_name"
    run systemctl restart "$unit_name"
  else
    log "[agent:$agent] restart skipped (--no-restart)"
  fi
done

if (( net_agent_selected == 1 )); then
  agent="rpi-net-agent"
  unit_name="${AGENT_UNITS[$agent]}"
  log "[agent:$agent] enable $unit_name"
  run systemctl enable "$unit_name"

  if (( NO_RESTART == 0 )); then
    log "[agent:$agent] restarting last (AP/STA switch may drop current SSH session)"
    log "[agent:$agent] if session drops, reconnect and run: /opt/unitlab/current/scripts/verify-host-agents.sh"
    run systemctl restart --no-block "$unit_name"
  else
    log "[agent:$agent] restart skipped (--no-restart)"
  fi
fi

echo
echo "Agent | Unit | Status | Install Path | Env Path"
echo "------|------|--------|--------------|---------"
for agent in "${SELECTED_AGENTS[@]}"; do
  unit_name="${AGENT_UNITS[$agent]}"
  status="(dry-run)"
  if (( DRY_RUN == 0 )); then
    status="inactive"
    if systemctl is-active --quiet "$unit_name"; then
      status="active"
    fi
  fi
  printf '%s | %s | %s | %s | %s\n' \
    "$agent" \
    "$unit_name" \
    "$status" \
    "$INSTALL_ROOT/$agent" \
    "$ENV_ROOT/$agent.env"
done

log "Completed"
