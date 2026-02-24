#!/usr/bin/env bash
set -euo pipefail

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

usage() {
  cat <<'EOF'
Usage:
  ./scripts/verify-host-agents.sh [--only <agent1,agent2>]

Checks systemd service status for host agents and returns non-zero when any is not active.
EOF
}

contains() {
  local needle="$1"; shift
  local item
  for item in "$@"; do
    [[ "$item" == "$needle" ]] && return 0
  done
  return 1
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
      echo "[unitlab] ERROR: unknown agent in --only: $agent" >&2
      exit 1
    fi
    if ! contains "$agent" "${SELECTED_AGENTS[@]:-}"; then
      SELECTED_AGENTS+=("$agent")
    fi
  done

  if [[ ${#SELECTED_AGENTS[@]} -eq 0 ]]; then
    echo "[unitlab] ERROR: --only resolved to empty list" >&2
    exit 1
  fi
}

normalize_selected_order() {
  local ordered=()
  local agent
  for agent in "${KNOWN_AGENTS[@]}"; do
    if contains "$agent" "${SELECTED_AGENTS[@]}"; then
      ordered+=("$agent")
    fi
  done
  SELECTED_AGENTS=("${ordered[@]}")
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --only)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --only requires a value" >&2; usage; exit 1; }
      ONLY_RAW="$2"
      shift 2
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

if [[ -n "$ONLY_RAW" ]]; then
  declare -a SELECTED_AGENTS=()
  parse_only "$ONLY_RAW"
else
  declare -a SELECTED_AGENTS=("${KNOWN_AGENTS[@]}")
fi

normalize_selected_order

if ! command -v systemctl >/dev/null 2>&1; then
  echo "[unitlab] ERROR: systemctl not found" >&2
  exit 2
fi

fail_count=0
for agent in "${SELECTED_AGENTS[@]}"; do
  unit_name="${AGENT_UNITS[$agent]}"
  if systemctl is-active --quiet "$unit_name"; then
    echo "[OK] $agent ($unit_name) active"
  else
    echo "[FAIL] $agent ($unit_name) inactive"
    fail_count=$((fail_count + 1))
  fi
done

if (( fail_count == 0 )); then
  echo "UNITLAB HOST AGENTS STATUS: HEALTHY"
  exit 0
fi

echo "UNITLAB HOST AGENTS STATUS: DEGRADED"
exit 1
