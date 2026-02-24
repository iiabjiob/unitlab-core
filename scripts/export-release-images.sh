#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-$ROOT_DIR/release-images.tar}"
BACKEND_IMAGE="${BACKEND_IMAGE:-unitlab-backend:latest}"
WEB_IMAGE="${WEB_IMAGE:-unitlab-web:latest}"
COMPRESS_EXPORT="${COMPRESS_EXPORT:-0}"

command -v docker >/dev/null 2>&1 || {
	echo "[unitlab] ERROR: docker not found" >&2
	exit 1
}

mkdir -p "$(dirname "$OUT_FILE")"

echo "[unitlab] Exporting images to: $OUT_FILE"
echo "[unitlab] Images: $BACKEND_IMAGE $WEB_IMAGE"

docker image inspect "$BACKEND_IMAGE" >/dev/null
docker image inspect "$WEB_IMAGE" >/dev/null

if [[ "$COMPRESS_EXPORT" == "1" ]]; then
	docker save "$BACKEND_IMAGE" "$WEB_IMAGE" | gzip -1 > "$OUT_FILE"
else
	docker save "$BACKEND_IMAGE" "$WEB_IMAGE" -o "$OUT_FILE"
fi

echo "[unitlab] OK: exported $OUT_FILE"

