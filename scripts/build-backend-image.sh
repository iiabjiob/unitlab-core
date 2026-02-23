#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BACKEND_IMAGE_TAG="${BACKEND_IMAGE_TAG:-${1:-unitlab-backend:latest}}"
TARGET_PLATFORM="${TARGET_PLATFORM:-linux/arm64}"
PUSH_IMAGE="${PUSH_IMAGE:-0}"

echo "[unitlab] Backend image tag: $BACKEND_IMAGE_TAG"
echo "[unitlab] Target platform: $TARGET_PLATFORM"

if ! docker buildx version >/dev/null 2>&1; then
  echo "[unitlab] ERROR: docker buildx is required (Docker Desktop / buildx plugin)." >&2
  exit 1
fi

if [[ "$PUSH_IMAGE" == "1" ]]; then
  docker buildx build \
    --platform "$TARGET_PLATFORM" \
    -f "$ROOT_DIR/backend/Dockerfile.prod" \
    -t "$BACKEND_IMAGE_TAG" \
    --push \
    "$ROOT_DIR/backend"
else
  docker buildx build \
    --platform "$TARGET_PLATFORM" \
    -f "$ROOT_DIR/backend/Dockerfile.prod" \
    -t "$BACKEND_IMAGE_TAG" \
    --load \
    "$ROOT_DIR/backend"
fi

echo "[unitlab] OK: built $BACKEND_IMAGE_TAG"
