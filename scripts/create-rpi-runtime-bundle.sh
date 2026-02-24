#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAMP="$(date +%Y%m%d-%H%M%S)"
RELEASE_VERSION="${RELEASE_VERSION:-$STAMP}"
UNITLAB_BACKEND_IMAGE="${UNITLAB_BACKEND_IMAGE:-unitlab-backend:${RELEASE_VERSION}}"
UNITLAB_WEB_IMAGE="${UNITLAB_WEB_IMAGE:-unitlab-web:${RELEASE_VERSION}}"
PROFILE="min"
OUT_DIR=""

usage() {
  cat <<'EOF'
Usage:
  create-rpi-runtime-bundle.sh [--profile min|service] [output_dir]

Profiles:
  min      Minimal production runtime bundle (default)
           Includes compose/config/host-services + env examples + deploy scripts.
  service  Service/debug-friendly bundle
           Adds docs and scripts.

Examples:
  ./scripts/create-rpi-runtime-bundle.sh
  ./scripts/create-rpi-runtime-bundle.sh --profile service
  ./scripts/create-rpi-runtime-bundle.sh /tmp/unitlab-core-rpi-runtime
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --profile requires a value" >&2; usage; exit 1; }
      PROFILE="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      if [[ -n "$OUT_DIR" ]]; then
        echo "[unitlab] ERROR: multiple output directories provided" >&2
        usage
        exit 1
      fi
      OUT_DIR="$1"
      shift
      ;;
  esac
done

if [[ "$PROFILE" != "min" && "$PROFILE" != "service" ]]; then
  echo "[unitlab] ERROR: unsupported profile '$PROFILE' (expected min|service)" >&2
  usage
  exit 1
fi

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$ROOT_DIR/dist-release/unitlab-core-rpi-runtime-${PROFILE}-${RELEASE_VERSION}"
fi

command -v rsync >/dev/null 2>&1 || {
  echo "[unitlab] ERROR: rsync not found" >&2
  exit 1
}

echo "[unitlab] Creating RPi runtime bundle at: $OUT_DIR (profile=$PROFILE)"
[[ -n "$OUT_DIR" && "$OUT_DIR" != "/" ]] || {
  echo "[unitlab] ERROR: invalid OUT_DIR" >&2
  exit 1
}
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

copy() {
  local src="$1"
  local dst="$2"
  mkdir -p "$(dirname "$dst")"
  rsync -a "$src" "$dst"
}

required_file() {
  local path="$1"
  [[ -f "$path" ]] || {
    echo "[unitlab] ERROR: missing required file: $path" >&2
    exit 1
  }
}

required_dir() {
  local path="$1"
  [[ -d "$path" ]] || {
    echo "[unitlab] ERROR: missing required directory: $path" >&2
    exit 1
  }
}

# Validate required inputs early for clear operator errors
required_file "$ROOT_DIR/docker-compose.prod.yml"
required_dir "$ROOT_DIR/config"
required_dir "$ROOT_DIR/host-services"

# Core runtime manifests/configs
copy "$ROOT_DIR/docker-compose.prod.yml" "$OUT_DIR/"
rsync -a \
  --exclude '*.dev.yml' \
  "$ROOT_DIR/config/" "$OUT_DIR/config/"

# Host services (installed on host via systemd)
rsync -a \
  --exclude '.venv' \
  --exclude '__pycache__' \
  --exclude '*.log' \
  --exclude '.DS_Store' \
  "$ROOT_DIR/host-services/" "$OUT_DIR/host-services/"

# Core deploy/runtime scripts (required on RPi in all profiles)
mkdir -p "$OUT_DIR/scripts"
copy "$ROOT_DIR/scripts/deploy-rpi.sh" "$OUT_DIR/scripts/"
copy "$ROOT_DIR/scripts/verify-rpi-runtime.sh" "$OUT_DIR/scripts/"
copy "$ROOT_DIR/scripts/cleanup-rpi-releases.sh" "$OUT_DIR/scripts/"
chmod +x \
  "$OUT_DIR/scripts/deploy-rpi.sh" \
  "$OUT_DIR/scripts/verify-rpi-runtime.sh" \
  "$OUT_DIR/scripts/cleanup-rpi-releases.sh"

# Optional service/debug extras
if [[ "$PROFILE" == "service" ]]; then
  # Ops scripts (including release helpers)
  rsync -a "$ROOT_DIR/scripts/" "$OUT_DIR/scripts/"

  # Docs useful during field deployment
  mkdir -p "$OUT_DIR/docs/guide"
  copy "$ROOT_DIR/docs/guide/rpi5-core-provisioning.md" "$OUT_DIR/docs/guide/"
  copy "$ROOT_DIR/docs/guide/frontend-web-image-release.md" "$OUT_DIR/docs/guide/"
fi

# Backend env templates (avoid shipping full backend source in runtime bundle)
mkdir -p "$OUT_DIR/backend"
cat > "$OUT_DIR/backend/.env.prod.example" <<'EOF'
APP_ENV=production
DEBUG=false
DEBUG_LEVEL=INFO

POSTGRES_USER=unitlab_pg_user
POSTGRES_PASSWORD=unitlab_pg_password
POSTGRES_HOST=db
POSTGRES_PORT=5432
POSTGRES_DB=unitlab_pg

REDIS_HOST=redis
REDIS_PORT=6379

MQTT_HOST=mosquitto
MQTT_PORT=1883
EOF

cat > "$OUT_DIR/backend/.env.db.prod.example" <<'EOF'
POSTGRES_USER=unitlab_pg_user
POSTGRES_PASSWORD=unitlab_pg_password
POSTGRES_DB=unitlab_pg
EOF

cat > "$OUT_DIR/.env.example" <<'EOF'
# Root compose env (same directory as docker-compose.prod.yml)
RELEASE_VERSION=2026.02.24-1345
# Backend/runtime app image (api + workers + migrations)
UNITLAB_BACKEND_IMAGE=unitlab-backend:${RELEASE_VERSION}
# Frontend web runtime image (nginx + built dist)
UNITLAB_WEB_IMAGE=unitlab-web:${RELEASE_VERSION}
EOF

cat > "$OUT_DIR/.env.release" <<EOF
RELEASE_VERSION=$RELEASE_VERSION
UNITLAB_BACKEND_IMAGE=$UNITLAB_BACKEND_IMAGE
UNITLAB_WEB_IMAGE=$UNITLAB_WEB_IMAGE
EOF

GIT_SHA="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
cat > "$OUT_DIR/RELEASE_INFO" <<EOF
PROFILE=$PROFILE
RELEASE_VERSION=$RELEASE_VERSION
BUILD_TIME=$STAMP
GIT_SHA=$GIT_SHA
EOF

cat > "$OUT_DIR/README_DEPLOY.md" <<'EOF'
# UnitLab RPi Runtime Deploy

1. Copy this bundle to `/opt/unitlab/releases/<bundle>` on RPi5.
2. Create/update symlink:
  ```bash
  ln -sfn /opt/unitlab/releases/<bundle> /opt/unitlab/current
  ```
3. Add env files from examples (`backend/.env.prod`, `backend/.env.db.prod`, `.env`).
4. Deploy:
  ```bash
  cd /opt/unitlab/current
  docker compose -f docker-compose.prod.yml pull
  docker compose -f docker-compose.prod.yml up -d
  ```

Rollback: point `/opt/unitlab/current` to previous release and rerun `docker compose ... up -d`.
EOF

echo
echo "[unitlab] Bundle created."
echo "[unitlab] Note: frontend source is intentionally omitted (image-based frontend delivery)."
if [[ "$PROFILE" == "service" ]]; then
  echo "[unitlab] Includes service/debug extras (docs + scripts)."
fi
echo "[unitlab] Next (recommended on RPi5):"
echo "[unitlab]   1) Copy bundle to: /opt/unitlab/releases/<bundle>"
echo "[unitlab]   2) Update symlink: ln -sfn /opt/unitlab/releases/<bundle> /opt/unitlab/current"
echo "[unitlab]   3) Deploy from symlink target: cd /opt/unitlab/current && docker compose -f docker-compose.prod.yml pull && docker compose -f docker-compose.prod.yml up -d"
echo "[unitlab] Rollback: repoint /opt/unitlab/current to previous release and run docker compose up -d"
