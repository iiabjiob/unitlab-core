#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-$ROOT_DIR/release-images.tar}"
BACKEND_IMAGE="${BACKEND_IMAGE:-unitlab-backend:latest}"
WEB_IMAGE="${WEB_IMAGE:-unitlab-web:latest}"

echo "[unitlab] Exporting images to: $OUT_FILE"
echo "[unitlab] Images: $BACKEND_IMAGE $WEB_IMAGE"

docker image inspect "$BACKEND_IMAGE" >/dev/null
docker image inspect "$WEB_IMAGE" >/dev/null

docker save "$BACKEND_IMAGE" "$WEB_IMAGE" -o "$OUT_FILE"

echo "[unitlab] OK: exported $OUT_FILE"

