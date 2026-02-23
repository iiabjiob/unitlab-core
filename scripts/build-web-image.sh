#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FRONTEND_DIR="$ROOT_DIR/frontend"

WEB_IMAGE_TAG="${WEB_IMAGE_TAG:-${1:-unitlab-web:latest}}"
SKIP_FRONTEND_BUILD="${SKIP_FRONTEND_BUILD:-0}"
TARGET_PLATFORM="${TARGET_PLATFORM:-linux/arm64}"
PUSH_IMAGE="${PUSH_IMAGE:-0}"

echo "[unitlab] Web image tag: $WEB_IMAGE_TAG"
echo "[unitlab] Target platform: $TARGET_PLATFORM"

if ! docker buildx version >/dev/null 2>&1; then
  echo "[unitlab] ERROR: docker buildx is required (Docker Desktop / buildx plugin)." >&2
  exit 1
fi

if [[ "$SKIP_FRONTEND_BUILD" != "1" ]]; then
  if ! command -v pnpm >/dev/null 2>&1; then
    echo "[unitlab] ERROR: pnpm is required to build frontend/dist" >&2
    exit 1
  fi
  echo "[unitlab] Building frontend dist..."
  (cd "$FRONTEND_DIR" && pnpm build)
fi

if [[ ! -f "$FRONTEND_DIR/dist/index.html" ]]; then
  echo "[unitlab] ERROR: frontend/dist/index.html not found. Build frontend first." >&2
  exit 1
fi

echo "[unitlab] Building web runtime image..."
if [[ "$PUSH_IMAGE" == "1" ]]; then
  docker buildx build \
    --platform "$TARGET_PLATFORM" \
    -f "$ROOT_DIR/Dockerfile.web.prod" \
    -t "$WEB_IMAGE_TAG" \
    --push \
    "$ROOT_DIR"
else
  docker buildx build \
    --platform "$TARGET_PLATFORM" \
    -f "$ROOT_DIR/Dockerfile.web.prod" \
    -t "$WEB_IMAGE_TAG" \
    --load \
    "$ROOT_DIR"
fi

echo "[unitlab] OK: built $WEB_IMAGE_TAG"
