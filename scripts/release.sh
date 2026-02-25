#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RELEASE_VERSION="${RELEASE_VERSION:-$(date +%Y.%m.%d-%H%M)}"
PROFILE="${PROFILE:-min}"
OFFLINE_EXPORT="${OFFLINE_EXPORT:-1}"
PUSH_IMAGE="${PUSH_IMAGE:-0}"
TARGET_PLATFORM="${TARGET_PLATFORM:-linux/arm64}"
BACKEND_IMAGE_TAG="${BACKEND_IMAGE_TAG:-unitlab-backend:${RELEASE_VERSION}}"
WEB_IMAGE_TAG="${WEB_IMAGE_TAG:-unitlab-web:${RELEASE_VERSION}}"
RELEASE_DIR="${RELEASE_DIR:-$ROOT_DIR/dist-release}"
BUNDLE_OUT="${BUNDLE_OUT:-}"
IMAGES_OUT="${IMAGES_OUT:-$RELEASE_DIR/release-images-${RELEASE_VERSION}.tar}"
COMPRESS_EXPORT="${COMPRESS_EXPORT:-0}"

usage() {
  cat <<'EOF'
Usage:
  release.sh [options]

Options:
  --profile <min|service>        Runtime bundle profile (default: min)
  --offline-export <0|1>         Export docker images tarball for offline deploy (default: 1)
  --compress-export <0|1>        Gzip image archive when offline export is enabled (default: 0)
  --push <0|1>                   Push images to registry instead of local-only load (default: 0)
  --platform <value>             Build target platform (default: linux/arm64)
  --release-version <value>      Unified release version (default: current datetime, e.g. 2026.02.24-1345)
  --backend-tag <image:tag>      Backend image tag (default: unitlab-backend:<release-version>)
  --web-tag <image:tag>          Web image tag (default: unitlab-web:<release-version>)
  --release-dir <path>           Directory for generated artifacts (default: ./dist-release)
  --bundle-out <path>            Exact output path for runtime bundle directory
  --images-out <path>            Exact output path for exported image archive
  -h, --help                     Show this help

Examples:
  # Default: local images + offline tar + min runtime bundle
  ./scripts/release.sh

  # Service bundle profile with compressed offline export
  ./scripts/release.sh --profile service --compress-export 1

  # Registry flow (push images), no offline tar
  ./scripts/release.sh --push 1 --offline-export 0 \
    --backend-tag ghcr.io/acme/unitlab-backend:2026.02.24 \
    --web-tag ghcr.io/acme/unitlab-web:2026.02.24
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --profile requires a value" >&2; usage; exit 1; }
      PROFILE="$2"
      shift 2
      ;;
    --offline-export)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --offline-export requires a value" >&2; usage; exit 1; }
      OFFLINE_EXPORT="$2"
      shift 2
      ;;
    --compress-export)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --compress-export requires a value" >&2; usage; exit 1; }
      COMPRESS_EXPORT="$2"
      shift 2
      ;;
    --push)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --push requires a value" >&2; usage; exit 1; }
      PUSH_IMAGE="$2"
      shift 2
      ;;
    --platform)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --platform requires a value" >&2; usage; exit 1; }
      TARGET_PLATFORM="$2"
      shift 2
      ;;
    --release-version)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --release-version requires a value" >&2; usage; exit 1; }
      RELEASE_VERSION="$2"
      shift 2
      ;;
    --backend-tag)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --backend-tag requires a value" >&2; usage; exit 1; }
      BACKEND_IMAGE_TAG="$2"
      shift 2
      ;;
    --web-tag)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --web-tag requires a value" >&2; usage; exit 1; }
      WEB_IMAGE_TAG="$2"
      shift 2
      ;;
    --release-dir)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --release-dir requires a value" >&2; usage; exit 1; }
      RELEASE_DIR="$2"
      shift 2
      ;;
    --bundle-out)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --bundle-out requires a value" >&2; usage; exit 1; }
      BUNDLE_OUT="$2"
      shift 2
      ;;
    --images-out)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --images-out requires a value" >&2; usage; exit 1; }
      IMAGES_OUT="$2"
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

if [[ "$PROFILE" != "min" && "$PROFILE" != "service" ]]; then
  echo "[unitlab] ERROR: unsupported profile '$PROFILE' (expected min|service)" >&2
  usage
  exit 1
fi

if [[ -z "$BUNDLE_OUT" ]]; then
  if [[ "$PROFILE" == "min" ]]; then
    BUNDLE_OUT="$RELEASE_DIR/unitlab-core-rpi-runtime-${RELEASE_VERSION}"
  else
    BUNDLE_OUT="$RELEASE_DIR/unitlab-core-rpi-runtime-${PROFILE}-${RELEASE_VERSION}"
  fi
fi

if [[ "$OFFLINE_EXPORT" != "0" && "$OFFLINE_EXPORT" != "1" ]]; then
  echo "[unitlab] ERROR: --offline-export must be 0 or 1" >&2
  exit 1
fi

if [[ "$COMPRESS_EXPORT" != "0" && "$COMPRESS_EXPORT" != "1" ]]; then
  echo "[unitlab] ERROR: --compress-export must be 0 or 1" >&2
  exit 1
fi

if [[ "$PUSH_IMAGE" != "0" && "$PUSH_IMAGE" != "1" ]]; then
  echo "[unitlab] ERROR: --push must be 0 or 1" >&2
  exit 1
fi

if [[ "$OFFLINE_EXPORT" == "0" && "$COMPRESS_EXPORT" == "1" ]]; then
  echo "[unitlab] WARN: --compress-export is ignored when --offline-export=0"
fi

if [[ "$PUSH_IMAGE" == "1" && "$OFFLINE_EXPORT" == "1" ]]; then
  echo "[unitlab] WARN: PUSH_IMAGE=1 with OFFLINE_EXPORT=1 (you usually want one delivery model)" >&2
fi

if [[ -z "$RELEASE_VERSION" ]]; then
  echo "[unitlab] ERROR: --release-version cannot be empty" >&2
  exit 1
fi

if [[ "$BACKEND_IMAGE_TAG" == "unitlab-backend:${RELEASE_VERSION}" || "$BACKEND_IMAGE_TAG" == "unitlab-backend:latest" ]]; then
  BACKEND_IMAGE_TAG="unitlab-backend:${RELEASE_VERSION}"
fi

if [[ "$WEB_IMAGE_TAG" == "unitlab-web:${RELEASE_VERSION}" || "$WEB_IMAGE_TAG" == "unitlab-web:latest" ]]; then
  WEB_IMAGE_TAG="unitlab-web:${RELEASE_VERSION}"
fi

if [[ "$BACKEND_IMAGE_TAG" == *":latest" ]]; then
  echo "[unitlab] WARN: backend tag uses :latest (not recommended for production)"
fi

if [[ "$WEB_IMAGE_TAG" == *":latest" ]]; then
  echo "[unitlab] WARN: web tag uses :latest (not recommended for production)"
fi

if git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  if [[ -n "$(git -C "$ROOT_DIR" status --porcelain)" ]]; then
    echo "[unitlab] WARN: working tree is dirty (uncommitted changes present)"
  fi
fi

command -v docker >/dev/null 2>&1 || {
  echo "[unitlab] ERROR: docker not found" >&2
  exit 1
}

[[ -n "$RELEASE_DIR" && "$RELEASE_DIR" != "/" ]] || {
  echo "[unitlab] ERROR: invalid RELEASE_DIR" >&2
  exit 1
}

[[ -n "$BUNDLE_OUT" && "$BUNDLE_OUT" != "/" ]] || {
  echo "[unitlab] ERROR: invalid BUNDLE_OUT" >&2
  exit 1
}

docker compose version >/dev/null 2>&1 || {
  echo "[unitlab] ERROR: docker compose plugin not found (docker compose version failed)" >&2
  exit 1
}

[[ -x "$ROOT_DIR/scripts/build-backend-image.sh" ]] || {
  echo "[unitlab] ERROR: missing or non-executable: $ROOT_DIR/scripts/build-backend-image.sh" >&2
  exit 1
}

[[ -x "$ROOT_DIR/scripts/build-web-image.sh" ]] || {
  echo "[unitlab] ERROR: missing or non-executable: $ROOT_DIR/scripts/build-web-image.sh" >&2
  exit 1
}

[[ -x "$ROOT_DIR/scripts/export-release-images.sh" ]] || {
  echo "[unitlab] ERROR: missing or non-executable: $ROOT_DIR/scripts/export-release-images.sh" >&2
  exit 1
}

[[ -x "$ROOT_DIR/scripts/create-rpi-runtime-bundle.sh" ]] || {
  echo "[unitlab] ERROR: missing or non-executable: $ROOT_DIR/scripts/create-rpi-runtime-bundle.sh" >&2
  exit 1
}

[[ -x "$ROOT_DIR/scripts/deploy-rpi.sh" ]] || {
  echo "[unitlab] ERROR: missing or non-executable: $ROOT_DIR/scripts/deploy-rpi.sh" >&2
  exit 1
}

docker buildx inspect >/dev/null 2>&1 || docker buildx create --use >/dev/null

mkdir -p "$RELEASE_DIR"

EXPORT_IMAGES_OUT="$IMAGES_OUT"
if [[ "$OFFLINE_EXPORT" == "1" && "$COMPRESS_EXPORT" == "1" && "$EXPORT_IMAGES_OUT" != *.gz ]]; then
  EXPORT_IMAGES_OUT="${EXPORT_IMAGES_OUT}.gz"
fi

EXPORT_IMAGES_DIR="$(dirname "$EXPORT_IMAGES_OUT")"
[[ -n "$EXPORT_IMAGES_DIR" && "$EXPORT_IMAGES_DIR" != "/" ]] || {
  echo "[unitlab] ERROR: invalid EXPORT_IMAGES_OUT directory" >&2
  exit 1
}

echo "[unitlab] Release pipeline started"
echo "[unitlab] BACKEND_IMAGE_TAG=$BACKEND_IMAGE_TAG"
echo "[unitlab] WEB_IMAGE_TAG=$WEB_IMAGE_TAG"
echo "[unitlab] RELEASE_VERSION=$RELEASE_VERSION"
echo "[unitlab] TARGET_PLATFORM=$TARGET_PLATFORM"
echo "[unitlab] PROFILE=$PROFILE"
echo "[unitlab] OFFLINE_EXPORT=$OFFLINE_EXPORT"
TOTAL_STEPS=4

echo "[unitlab] Step 1/$TOTAL_STEPS: build backend image"
TARGET_PLATFORM="$TARGET_PLATFORM" PUSH_IMAGE="$PUSH_IMAGE" \
  BACKEND_IMAGE_TAG="$BACKEND_IMAGE_TAG" \
  "$ROOT_DIR/scripts/build-backend-image.sh" "$BACKEND_IMAGE_TAG"

echo "[unitlab] Step 2/$TOTAL_STEPS: build web image"
TARGET_PLATFORM="$TARGET_PLATFORM" PUSH_IMAGE="$PUSH_IMAGE" \
  WEB_IMAGE_TAG="$WEB_IMAGE_TAG" \
  "$ROOT_DIR/scripts/build-web-image.sh" "$WEB_IMAGE_TAG"

echo "[unitlab] Built images:"
if [[ "$PUSH_IMAGE" == "1" ]]; then
  echo "[unitlab] Note: images were pushed; local image IDs may be unavailable."
fi
docker image inspect "$BACKEND_IMAGE_TAG" --format='[unitlab] backend={{.Id}}' 2>/dev/null || true
docker image inspect "$WEB_IMAGE_TAG" --format='[unitlab] web={{.Id}}' 2>/dev/null || true

if [[ "$OFFLINE_EXPORT" == "1" ]]; then
  echo "[unitlab] Step 3/$TOTAL_STEPS: export offline image archive"
  BACKEND_IMAGE="$BACKEND_IMAGE_TAG" WEB_IMAGE="$WEB_IMAGE_TAG" COMPRESS_EXPORT="$COMPRESS_EXPORT" \
    "$ROOT_DIR/scripts/export-release-images.sh" "$EXPORT_IMAGES_OUT"
else
  echo "[unitlab] Step 3/$TOTAL_STEPS: offline export skipped (OFFLINE_EXPORT=$OFFLINE_EXPORT)"
fi

echo "[unitlab] Step 4/$TOTAL_STEPS: create runtime bundle"
RELEASE_VERSION="$RELEASE_VERSION" \
UNITLAB_BACKEND_IMAGE="$BACKEND_IMAGE_TAG" \
UNITLAB_WEB_IMAGE="$WEB_IMAGE_TAG" \
"$ROOT_DIR/scripts/create-rpi-runtime-bundle.sh" --profile "$PROFILE" "$BUNDLE_OUT"

echo
echo "[unitlab] Release pipeline complete"
echo "[unitlab] Runtime bundle: $BUNDLE_OUT"
if [[ "$OFFLINE_EXPORT" == "1" ]]; then
  echo "[unitlab] Image archive: $EXPORT_IMAGES_OUT"
fi
echo "[unitlab] Next on RPi5: run deploy script (verify/cleanup live there):"
if [[ "$OFFLINE_EXPORT" == "1" ]]; then
  echo "[unitlab]   sudo /opt/unitlab/releases/$(basename "$BUNDLE_OUT")/scripts/deploy-rpi.sh --bundle-dir /opt/unitlab/releases/$(basename "$BUNDLE_OUT") --images-archive /opt/unitlab/$(basename "$EXPORT_IMAGES_OUT")"
else
  echo "[unitlab]   sudo /opt/unitlab/releases/$(basename "$BUNDLE_OUT")/scripts/deploy-rpi.sh --bundle-dir /opt/unitlab/releases/$(basename "$BUNDLE_OUT")"
fi
