#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${1:-$ROOT_DIR/dist-release/unitlab-core-rpi-runtime-$STAMP}"

echo "[unitlab] Creating RPi runtime bundle at: $OUT_DIR"
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

copy() {
  local src="$1"
  local dst="$2"
  mkdir -p "$(dirname "$dst")"
  rsync -a "$src" "$dst"
}

# Core runtime manifests/configs
copy "$ROOT_DIR/docker-compose.prod.yml" "$OUT_DIR/"
copy "$ROOT_DIR/config/" "$OUT_DIR/config/"

# Backend source (still needed if backend image is built on-device)
rsync -a \
  --exclude '.venv' \
  --exclude '__pycache__' \
  --exclude '.pytest_cache' \
  --exclude '.mypy_cache' \
  "$ROOT_DIR/backend/" "$OUT_DIR/backend/"

# Host services (installed on host via systemd)
rsync -a \
  --exclude '.venv' \
  --exclude '__pycache__' \
  "$ROOT_DIR/host-services/" "$OUT_DIR/host-services/"

# Ops scripts (including host installers + release helpers)
rsync -a "$ROOT_DIR/scripts/" "$OUT_DIR/scripts/"

# Docs useful during field deployment
mkdir -p "$OUT_DIR/docs/guide"
copy "$ROOT_DIR/docs/guide/rpi5-core-provisioning.md" "$OUT_DIR/docs/guide/"
copy "$ROOT_DIR/docs/guide/frontend-web-image-release.md" "$OUT_DIR/docs/guide/"

cat > "$OUT_DIR/.env.example" <<'EOF'
# Root compose env (same directory as docker-compose.prod.yml)
# Backend/runtime app image (api + workers + migrations)
UNITLAB_BACKEND_IMAGE=unitlab-backend:latest
# Frontend web runtime image (nginx + built dist)
UNITLAB_WEB_IMAGE=unitlab-web:latest
EOF

echo
echo "[unitlab] Bundle created."
echo "[unitlab] Note: frontend source is intentionally omitted (image-based frontend delivery)."
echo "[unitlab] Next: copy bundle to /opt/unitlab/unitlab-core on RPi5, add env files, load/pull images, docker compose up -d --build"
