#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/unitlab-rpi5-release-test.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

runtime="$WORK_DIR/runtime"
mkdir -p "$runtime/shared" "$runtime/scripts" "$runtime/host-services" "$runtime/host-agent-wheels"
touch "$runtime/host-agent-wheels/unitlab_rpi_net_agent-0.0.0-py3-none-any.whl"
touch "$runtime/host-agent-wheels/unitlab_rpi_ntp_agent-0.0.0-py3-none-any.whl"
touch "$runtime/host-agent-wheels/unitlab_rpi_core_diag_agent-0.0.0-py3-none-any.whl"
touch "$runtime/host-agent-wheels/unitlab_rpi_provision_agent-0.0.0-py3-none-any.whl"
touch "$runtime/host-agent-wheels/redis-0.0.0-py3-none-any.whl"
printf 'RELEASE_VERSION=2026.09.15-test\n' > "$runtime/RELEASE_INFO"
printf 'RELEASE_VERSION=2026.09.15-test\nUNITLAB_BACKEND_IMAGE=unitlab-backend:2026.09.15-test\nUNITLAB_WEB_IMAGE=unitlab-web:2026.09.15-test\n' > "$runtime/.env.release"
touch "$runtime/docker-compose.prod.yml" "$runtime/shared/backend.env.example" "$runtime/shared/db.env.example"
for script in deploy-rpi.sh install-host-agents.sh verify-host-agents.sh verify-rpi-runtime.sh unitlab; do
  touch "$runtime/scripts/$script"
done
tar -cf "$WORK_DIR/images.tar" -C "$WORK_DIR" runtime

output="$WORK_DIR/unitlab-core-rpi5-test.tar.gz"
"$ROOT_DIR/scripts/package-rpi5-release.sh" "$runtime" "$WORK_DIR/images.tar" "$output" >/dev/null
tar -tzf "$output" > "$WORK_DIR/contents"
for path in manifest.json checksums.sha256 runtime/docker-compose.prod.yml runtime/shared/backend.env.example runtime/scripts/unitlab images/images.tar; do
  grep -Fxq "$path" "$WORK_DIR/contents"
done
if grep -q 'frontend/src' "$WORK_DIR/contents"; then
  echo "frontend source leaked into release archive" >&2
  exit 1
fi

echo "RPi5 release archive checks passed"
