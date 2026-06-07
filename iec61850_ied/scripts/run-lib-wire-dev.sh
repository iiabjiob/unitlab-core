#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="/workspace/iec61850_ied"
BUILD_DIR="$PROJECT_DIR/build-libiec61850"
BIN="$BUILD_DIR/unitlab-iec61850-ied-sim"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "build-libiec61850 is not configured. Configure it with UNITLAB_IEC61850_SIM_WITH_LIBIEC61850=ON first."
  exit 1
fi

if [ ! -x "$BIN" ]; then
  echo "missing binary: $BIN"
  exit 1
fi

stop_listeners_on_port() {
  local port="$1"
  local pids=""

  if command -v lsof >/dev/null 2>&1; then
    pids="$(lsof -tiTCP:"$port" -sTCP:LISTEN 2>/dev/null || true)"
  elif command -v ss >/dev/null 2>&1; then
    pids="$(ss -ltnp "sport = :$port" 2>/dev/null | sed -n 's/.*pid=\([0-9][0-9]*\).*/\1/p' | sort -u || true)"
  fi

  if [ -n "$pids" ]; then
    echo "stopping listener on port $port: $pids"
    printf '%s\n' "$pids" | xargs -r kill 2>/dev/null || true
    sleep 1
    printf '%s\n' "$pids" | xargs -r kill -9 2>/dev/null || true
  fi
}

echo "building..."
cmake --build "$BUILD_DIR" --target unitlab-iec61850-ied-sim -j8

echo "stopping previous server..."
stop_listeners_on_port 12448

echo "starting libiec61850-backed server..."
exec "$BIN" \
  --fixture "$PROJECT_DIR/examples/single-report.fixture.json" \
  --ied IED1 \
  --bind 0.0.0.0 \
  --port 12448
