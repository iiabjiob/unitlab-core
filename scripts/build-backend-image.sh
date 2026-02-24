#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RELEASE_VERSION="${RELEASE_VERSION:-$(date +%Y.%m.%d-%H%M)}"
BACKEND_IMAGE_TAG="${BACKEND_IMAGE_TAG:-${1:-unitlab-backend:${RELEASE_VERSION}}}"
TARGET_PLATFORM="${TARGET_PLATFORM:-linux/arm64}"
PUSH_IMAGE="${PUSH_IMAGE:-0}"
DOCKERFILE_PATH="$ROOT_DIR/backend/Dockerfile.prod"

echo "[unitlab] Backend image tag: $BACKEND_IMAGE_TAG"
echo "[unitlab] Target platform: $TARGET_PLATFORM"

if ! docker buildx version >/dev/null 2>&1; then
  echo "[unitlab] ERROR: docker buildx is required (Docker Desktop / buildx plugin)." >&2
  exit 1
fi

docker buildx inspect >/dev/null 2>&1 || docker buildx create --use >/dev/null

[[ -f "$DOCKERFILE_PATH" ]] || {
  echo "[unitlab] ERROR: Dockerfile not found: $DOCKERFILE_PATH" >&2
  exit 1
}

if [[ "$PUSH_IMAGE" == "1" ]]; then
  docker buildx build \
    --platform "$TARGET_PLATFORM" \
    -f "$DOCKERFILE_PATH" \
    -t "$BACKEND_IMAGE_TAG" \
    --push \
    "$ROOT_DIR/backend"
else
  docker buildx build \
    --platform "$TARGET_PLATFORM" \
    -f "$DOCKERFILE_PATH" \
    -t "$BACKEND_IMAGE_TAG" \
    --load \
    "$ROOT_DIR/backend"
fi

echo "[unitlab] OK: built $BACKEND_IMAGE_TAG"
