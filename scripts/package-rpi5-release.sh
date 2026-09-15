#!/usr/bin/env bash
set -euo pipefail

RUNTIME_BUNDLE="${1:-}"
IMAGES_ARCHIVE="${2:-}"
OUTPUT_ARCHIVE="${3:-}"

usage() {
  cat <<'EOF'
Usage:
  package-rpi5-release.sh <runtime-bundle-dir> <images.tar[.gz]> <output.tar.gz>

Creates the portable UnitLab Core RPi5 release archive.
EOF
}

if [[ -z "$RUNTIME_BUNDLE" || -z "$IMAGES_ARCHIVE" || -z "$OUTPUT_ARCHIVE" ]]; then
  usage >&2
  exit 1
fi

[[ -d "$RUNTIME_BUNDLE" ]] || { echo "[unitlab] ERROR: runtime bundle not found: $RUNTIME_BUNDLE" >&2; exit 1; }
[[ -f "$IMAGES_ARCHIVE" ]] || { echo "[unitlab] ERROR: image archive not found: $IMAGES_ARCHIVE" >&2; exit 1; }
command -v tar >/dev/null 2>&1 || { echo "[unitlab] ERROR: tar is required" >&2; exit 1; }
command -v gzip >/dev/null 2>&1 || { echo "[unitlab] ERROR: gzip is required" >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "[unitlab] ERROR: sha256sum is required" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "[unitlab] ERROR: python3 is required" >&2; exit 1; }

release_version=""
if [[ -f "$RUNTIME_BUNDLE/RELEASE_INFO" ]]; then
  release_version="$(sed -n 's/^RELEASE_VERSION=//p' "$RUNTIME_BUNDLE/RELEASE_INFO" | head -n1)"
fi
if [[ -z "$release_version" && -f "$RUNTIME_BUNDLE/.env.release" ]]; then
  release_version="$(sed -n 's/^RELEASE_VERSION=//p' "$RUNTIME_BUNDLE/.env.release" | head -n1)"
fi
[[ -n "$release_version" ]] || { echo "[unitlab] ERROR: RELEASE_VERSION missing from runtime bundle" >&2; exit 1; }

backend_image="$(sed -n 's/^UNITLAB_BACKEND_IMAGE=//p' "$RUNTIME_BUNDLE/.env.release" | head -n1)"
web_image="$(sed -n 's/^UNITLAB_WEB_IMAGE=//p' "$RUNTIME_BUNDLE/.env.release" | head -n1)"
[[ -n "$backend_image" && -n "$web_image" ]] || { echo "[unitlab] ERROR: image names missing from .env.release" >&2; exit 1; }

work_dir="$(mktemp -d "${TMPDIR:-/tmp}/unitlab-rpi5-release.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT
mkdir -p "$work_dir/runtime" "$work_dir/images"

rsync -a --delete "$RUNTIME_BUNDLE/" "$work_dir/runtime/"
if [[ "$IMAGES_ARCHIVE" == *.gz ]]; then
  gzip -cd "$IMAGES_ARCHIVE" > "$work_dir/images/images.tar"
else
  cp "$IMAGES_ARCHIVE" "$work_dir/images/images.tar"
fi

# Keep the requested portable layout while retaining host-service sources for
# the existing installer and systemd/chrony assets for field inspection.
mkdir -p "$work_dir/runtime/host-agent-wheels" "$work_dir/runtime/systemd"
if [[ -d "$work_dir/runtime/wheels" ]]; then
  rsync -a "$work_dir/runtime/wheels/" "$work_dir/runtime/host-agent-wheels/"
  rm -rf "$work_dir/runtime/wheels"
fi
if [[ -d "$work_dir/runtime/host-services" ]]; then
  (
    cd "$work_dir/runtime/host-services"
    find . -type f \( -path '*/systemd/*.service' -o -path '*/chrony/*' \) -exec cp --parents {} "$work_dir/runtime/systemd/" \;
  )
fi

python3 - "$work_dir/manifest.json" "$release_version" "$backend_image" "$web_image" <<'PY'
import json
import sys
from datetime import datetime, timezone

path, version, backend, web = sys.argv[1:]
manifest = {
    "product": "unitlab-core",
    "version": version,
    "platform": "linux/arm64",
    "created_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    "compose_project": "unitlab",
    "images": [backend, web],
    "requires": {"docker": ">=24", "compose": ">=2"},
}
with open(path, "w", encoding="utf-8") as handle:
    json.dump(manifest, handle, indent=2)
    handle.write("\n")
PY

(cd "$work_dir" && find runtime images -type f -print0 | sort -z | xargs -0 sha256sum > checksums.sha256)

mkdir -p "$(dirname "$OUTPUT_ARCHIVE")"
rm -f "$OUTPUT_ARCHIVE"
tar -C "$work_dir" -czf "$OUTPUT_ARCHIVE" manifest.json checksums.sha256 runtime images
echo "[unitlab] RPi5 release archive: $OUTPUT_ARCHIVE"
