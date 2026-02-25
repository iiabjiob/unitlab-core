#!/usr/bin/env bash
set -euo pipefail

BUNDLE_DIR=""
IMAGES_ARCHIVE=""
RELEASES_DIR="${RELEASES_DIR:-/opt/unitlab/releases}"
CURRENT_LINK="${CURRENT_LINK:-/opt/unitlab/current}"
COMPOSE_FILE_NAME="${COMPOSE_FILE_NAME:-docker-compose.prod.yml}"
COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-unitlab}"
VERIFY_RUNTIME="${VERIFY_RUNTIME:-1}"
CLEANUP_RELEASES="${CLEANUP_RELEASES:-1}"

load_release_env() {
  local release_dir="$1"
  unset RELEASE_VERSION UNITLAB_BACKEND_IMAGE UNITLAB_WEB_IMAGE
  if [[ -f "$release_dir/.env.release" ]]; then
    set -a
    . "$release_dir/.env.release"
    set +a
    echo "[unitlab] Loaded release env: $release_dir/.env.release"
  fi
}

usage() {
  cat <<'EOF'
Usage:
  sudo ./scripts/deploy-rpi.sh --bundle-dir <path> [options]

Required:
  --bundle-dir <path>            Path to prepared runtime bundle directory

Options:
  --images-archive <path>        Optional docker image archive (.tar/.tar.gz)
  --releases-dir <path>          Releases directory (default: /opt/unitlab/releases)
  --current-link <path>          Current symlink path (default: /opt/unitlab/current)
  --compose-file-name <name>     Compose file inside bundle (default: docker-compose.prod.yml)
  --compose-project-name <name>  Compose project name (default: unitlab)
  --verify <0|1>                 Run runtime verification after deploy (default: 1)
  --cleanup-releases <0|1>       Cleanup old releases after successful verify (default: 1)
  -h, --help                     Show this help

Environment overrides:
  RELEASES_DIR, CURRENT_LINK, COMPOSE_FILE_NAME, COMPOSE_PROJECT_NAME, VERIFY_RUNTIME, CLEANUP_RELEASES
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bundle-dir)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --bundle-dir requires a value" >&2; usage; exit 1; }
      BUNDLE_DIR="$2"
      shift 2
      ;;
    --images-archive)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --images-archive requires a value" >&2; usage; exit 1; }
      IMAGES_ARCHIVE="$2"
      shift 2
      ;;
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
    --compose-file-name)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --compose-file-name requires a value" >&2; usage; exit 1; }
      COMPOSE_FILE_NAME="$2"
      shift 2
      ;;
    --compose-project-name)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --compose-project-name requires a value" >&2; usage; exit 1; }
      COMPOSE_PROJECT_NAME="$2"
      shift 2
      ;;
    --verify)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --verify requires a value" >&2; usage; exit 1; }
      VERIFY_RUNTIME="$2"
      shift 2
      ;;
    --cleanup-releases)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --cleanup-releases requires a value" >&2; usage; exit 1; }
      CLEANUP_RELEASES="$2"
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

if [[ -z "$BUNDLE_DIR" ]]; then
  echo "[unitlab] ERROR: --bundle-dir is required" >&2
  usage
  exit 1
fi

if [[ "$VERIFY_RUNTIME" != "0" && "$VERIFY_RUNTIME" != "1" ]]; then
  echo "[unitlab] ERROR: --verify must be 0 or 1" >&2
  exit 1
fi

if [[ "$CLEANUP_RELEASES" != "0" && "$CLEANUP_RELEASES" != "1" ]]; then
  echo "[unitlab] ERROR: --cleanup-releases must be 0 or 1" >&2
  exit 1
fi

if [[ "$CLEANUP_RELEASES" == "1" && "$VERIFY_RUNTIME" != "1" ]]; then
  echo "[unitlab] ERROR: --cleanup-releases requires --verify 1" >&2
  exit 1
fi

if [[ "$EUID" -ne 0 ]]; then
  echo "[unitlab] ERROR: run as root (use sudo)." >&2
  exit 1
fi

command -v docker >/dev/null 2>&1 || { echo "[unitlab] ERROR: docker not found" >&2; exit 1; }
docker compose version >/dev/null 2>&1 || { echo "[unitlab] ERROR: docker compose plugin not found" >&2; exit 1; }

[[ -n "$RELEASES_DIR" && "$RELEASES_DIR" != "/" ]] || { echo "[unitlab] ERROR: invalid RELEASES_DIR" >&2; exit 1; }
[[ -n "$CURRENT_LINK" && "$CURRENT_LINK" != "/" ]] || { echo "[unitlab] ERROR: invalid CURRENT_LINK" >&2; exit 1; }
[[ -n "$COMPOSE_PROJECT_NAME" ]] || { echo "[unitlab] ERROR: invalid COMPOSE_PROJECT_NAME" >&2; exit 1; }

if [[ ! -d "$BUNDLE_DIR" ]]; then
  echo "[unitlab] ERROR: bundle directory not found: $BUNDLE_DIR" >&2
  exit 1
fi

if [[ ! -f "$BUNDLE_DIR/$COMPOSE_FILE_NAME" ]]; then
  echo "[unitlab] ERROR: compose file missing in bundle: $BUNDLE_DIR/$COMPOSE_FILE_NAME" >&2
  exit 1
fi

required_env_files=(
  /opt/unitlab/shared/backend.env
  /opt/unitlab/shared/db.env
)
for env_file in "${required_env_files[@]}"; do
  if [[ ! -f "$env_file" ]]; then
    echo "[unitlab] ERROR: required env file missing: $env_file" >&2
    echo "[unitlab] HINT: initialize from bundle examples, then edit secrets:" >&2
    echo "[unitlab]   sudo mkdir -p /opt/unitlab/shared" >&2
    echo "[unitlab]   sudo cp -n $BUNDLE_DIR/shared/backend.env.example /opt/unitlab/shared/backend.env" >&2
    echo "[unitlab]   sudo cp -n $BUNDLE_DIR/shared/db.env.example /opt/unitlab/shared/db.env" >&2
    exit 1
  fi
done

if [[ -n "$IMAGES_ARCHIVE" && ! -f "$IMAGES_ARCHIVE" ]]; then
  echo "[unitlab] ERROR: image archive not found: $IMAGES_ARCHIVE" >&2
  exit 1
fi

if [[ "$VERIFY_RUNTIME" == "1" ]]; then
  [[ -x "$BUNDLE_DIR/scripts/verify-rpi-runtime.sh" ]] || {
    echo "[unitlab] ERROR: verify script missing in bundle: $BUNDLE_DIR/scripts/verify-rpi-runtime.sh" >&2
    exit 1
  }
fi

if [[ "$CLEANUP_RELEASES" == "1" ]]; then
  [[ -x "$BUNDLE_DIR/scripts/cleanup-rpi-releases.sh" ]] || {
    echo "[unitlab] ERROR: cleanup script missing in bundle: $BUNDLE_DIR/scripts/cleanup-rpi-releases.sh" >&2
    exit 1
  }
fi

release_name="$(basename "$BUNDLE_DIR")"
target_release="$RELEASES_DIR/$release_name"
resolved_bundle_dir="$(readlink -f "$BUNDLE_DIR")"
resolved_target_release="$(readlink -m "$target_release")"
previous_target=""
if [[ -L "$CURRENT_LINK" ]]; then
  previous_target="$(readlink -f "$CURRENT_LINK")"
fi

if [[ -n "$previous_target" && "$resolved_target_release" == "$previous_target" ]]; then
  if [[ "$resolved_bundle_dir" == "$resolved_target_release" ]]; then
    echo "[unitlab] Retrying deploy on already active release: $target_release"
  else
    echo "[unitlab] ERROR: trying to overwrite active release from different source: $target_release" >&2
    exit 1
  fi
fi

echo "[unitlab] Deploy release: $release_name"
mkdir -p "$RELEASES_DIR"
if [[ "$resolved_bundle_dir" == "$resolved_target_release" ]]; then
  echo "[unitlab] Bundle already in releases dir; skipping bundle copy"
else
  rm -rf "$target_release"
  mkdir -p "$target_release"
  rsync -a --delete "$BUNDLE_DIR/" "$target_release/"
fi

if [[ -n "$IMAGES_ARCHIVE" ]]; then
  echo "[unitlab] Loading images: $IMAGES_ARCHIVE"
  docker load -i "$IMAGES_ARCHIVE"
fi

echo "[unitlab] Validating compose config in target release"
load_release_env "$target_release"
docker compose --project-directory "$target_release" -f "$target_release/$COMPOSE_FILE_NAME" config >/dev/null

echo "[unitlab] Switching current symlink"
ln -sfn "$target_release" "$CURRENT_LINK"

compose_file="$CURRENT_LINK/$COMPOSE_FILE_NAME"
load_release_env "$CURRENT_LINK"
if [[ -n "$IMAGES_ARCHIVE" ]]; then
  echo "[unitlab] Starting stack from loaded images"
  docker compose --project-name "$COMPOSE_PROJECT_NAME" --project-directory "$CURRENT_LINK" -f "$compose_file" up -d --remove-orphans
else
  echo "[unitlab] Pulling images and starting stack"
  docker compose --project-name "$COMPOSE_PROJECT_NAME" --project-directory "$CURRENT_LINK" -f "$compose_file" pull
  docker compose --project-name "$COMPOSE_PROJECT_NAME" --project-directory "$CURRENT_LINK" -f "$compose_file" up -d --remove-orphans
fi

if [[ "$VERIFY_RUNTIME" == "1" ]]; then
  echo "[unitlab] Running runtime verification"
  expected_release_version=""
  if [[ -f "$CURRENT_LINK/RELEASE_INFO" ]]; then
    expected_release_version="$(grep -E '^RELEASE_VERSION=' "$CURRENT_LINK/RELEASE_INFO" | head -n1 | cut -d'=' -f2- || true)"
  fi
  if [[ -n "$expected_release_version" ]]; then
    verify_args=(--release-version "$expected_release_version")
  else
    verify_args=()
  fi
  if ! "$CURRENT_LINK/scripts/verify-rpi-runtime.sh" --project-dir "$CURRENT_LINK" --compose-file "$compose_file" "${verify_args[@]}"; then
    echo "[unitlab] Verify failed, rolling back"
    if [[ -n "$previous_target" && -d "$previous_target" ]]; then
      ln -sfn "$previous_target" "$CURRENT_LINK"
      load_release_env "$CURRENT_LINK"
      docker compose --project-name "$COMPOSE_PROJECT_NAME" --project-directory "$CURRENT_LINK" -f "$CURRENT_LINK/$COMPOSE_FILE_NAME" up -d --remove-orphans
      echo "[unitlab] Rolled back to previous release: $previous_target"
    else
      echo "[unitlab] WARN: previous release not available; rollback skipped" >&2
    fi
    exit 1
  fi
fi

if [[ "$CLEANUP_RELEASES" == "1" ]]; then
  echo "[unitlab] Cleaning up old releases"
  "$CURRENT_LINK/scripts/cleanup-rpi-releases.sh" --releases-dir "$RELEASES_DIR" --current-link "$CURRENT_LINK"
fi

echo "[unitlab] Deploy complete"
echo "[unitlab] Active release:"
readlink -f "$CURRENT_LINK"
