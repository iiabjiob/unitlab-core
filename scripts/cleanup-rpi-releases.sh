#!/usr/bin/env bash
set -euo pipefail

RELEASES_DIR="${RELEASES_DIR:-/opt/unitlab/releases}"
CURRENT_LINK="${CURRENT_LINK:-/opt/unitlab/current}"
DRY_RUN="${DRY_RUN:-0}"

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/cleanup-rpi-releases.sh [--releases-dir <path>] [--current-link <path>] [--dry-run]

Behavior:
  - Keeps current release (target of current symlink)
  - Keeps one previous release (most recent one that is not current)
  - Removes older release directories from releases-dir

Options:
  --releases-dir <path>   Releases directory (default: /opt/unitlab/releases)
  --current-link <path>   Symlink to current release (default: /opt/unitlab/current)
  --dry-run               Show what would be removed without deleting
  -h, --help              Show this help

Environment overrides:
  RELEASES_DIR, CURRENT_LINK, DRY_RUN
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --releases-dir)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --releases-dir requires a value" >&2; usage; exit 1; }
      RELEASES_DIR="$2"
      shift 2
      ;;
    --current-link)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --current-link requires a value" >&2; usage; exit 1; }
      CURRENT_LINK="$2"
      shift 2
      ;;
    --dry-run)
      DRY_RUN="1"
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

if [[ "$DRY_RUN" != "0" && "$DRY_RUN" != "1" ]]; then
  echo "[unitlab] ERROR: DRY_RUN must be 0 or 1" >&2
  exit 1
fi

[[ -n "$RELEASES_DIR" && "$RELEASES_DIR" != "/" ]] || {
  echo "[unitlab] ERROR: invalid RELEASES_DIR" >&2
  exit 1
}

[[ -n "$CURRENT_LINK" && "$CURRENT_LINK" != "/" ]] || {
  echo "[unitlab] ERROR: invalid CURRENT_LINK" >&2
  exit 1
}

if [[ ! -d "$RELEASES_DIR" ]]; then
  echo "[unitlab] ERROR: releases directory not found: $RELEASES_DIR" >&2
  exit 1
fi

if [[ ! -L "$CURRENT_LINK" ]]; then
  echo "[unitlab] ERROR: current symlink not found: $CURRENT_LINK" >&2
  exit 1
fi

current_target="$(readlink -f "$CURRENT_LINK")"
current_name="$(basename "$current_target")"

if [[ ! -d "$RELEASES_DIR/$current_name" ]]; then
  echo "[unitlab] ERROR: current target is outside releases dir or missing: $current_target" >&2
  exit 1
fi

echo "[unitlab] Current release: $current_name"

mapfile -t releases < <(find "$RELEASES_DIR" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort -r)

if [[ ${#releases[@]} -eq 0 ]]; then
  echo "[unitlab] Nothing to cleanup: no release directories found"
  exit 0
fi

keep=("$current_name")
for release_name in "${releases[@]}"; do
  if [[ "$release_name" != "$current_name" ]]; then
    keep+=("$release_name")
    break
  fi
done

echo "[unitlab] Keeping:"
printf '  %s\n' "${keep[@]}"

contains() {
  local value="$1"
  shift
  for item in "$@"; do
    if [[ "$item" == "$value" ]]; then
      return 0
    fi
  done
  return 1
}

removed=0
for release_name in "${releases[@]}"; do
  if contains "$release_name" "${keep[@]}"; then
    continue
  fi

  release_path="$RELEASES_DIR/$release_name"
  if [[ ! -d "$release_path" ]]; then
    continue
  fi

  if [[ "$DRY_RUN" == "1" ]]; then
    echo "[unitlab] DRY-RUN remove: $release_path"
  else
    echo "[unitlab] Removing old release: $release_name"
    rm -rf -- "$release_path"
  fi
  removed=$((removed + 1))
done

if [[ "$DRY_RUN" == "1" ]]; then
  echo "[unitlab] Cleanup dry-run complete. Candidates: $removed"
else
  echo "[unitlab] Cleanup complete. Removed: $removed"
fi
