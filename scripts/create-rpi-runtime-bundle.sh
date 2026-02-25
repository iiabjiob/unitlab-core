#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAMP="$(date +%Y%m%d-%H%M%S)"
RELEASE_VERSION="${RELEASE_VERSION:-$STAMP}"
UNITLAB_BACKEND_IMAGE="${UNITLAB_BACKEND_IMAGE:-unitlab-backend:${RELEASE_VERSION}}"
UNITLAB_WEB_IMAGE="${UNITLAB_WEB_IMAGE:-unitlab-web:${RELEASE_VERSION}}"
PROFILE="min"
OUT_DIR=""
BUILD_HOST_AGENT_WHEELS="${BUILD_HOST_AGENT_WHEELS:-1}"

usage() {
  cat <<'EOF'
Usage:
  create-rpi-runtime-bundle.sh [--profile min|service] [output_dir]

Options:
  --skip-host-agent-wheels   Skip wheelhouse build (not recommended)

Environment overrides:
  BUILD_HOST_AGENT_WHEELS=0|1
  PYTHON_BIN=<python interpreter used for wheel build>

Profiles:
  min      Minimal production runtime bundle (default)
           Includes compose/config/host-services + env examples + deploy scripts.
  service  Service/debug-friendly bundle
           Adds docs and scripts.

Examples:
  ./scripts/create-rpi-runtime-bundle.sh
  ./scripts/create-rpi-runtime-bundle.sh --profile service
  ./scripts/create-rpi-runtime-bundle.sh /tmp/unitlab-core-rpi-runtime
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --profile requires a value" >&2; usage; exit 1; }
      PROFILE="$2"
      shift 2
      ;;
    --skip-host-agent-wheels)
      BUILD_HOST_AGENT_WHEELS="0"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      if [[ -n "$OUT_DIR" ]]; then
        echo "[unitlab] ERROR: multiple output directories provided" >&2
        usage
        exit 1
      fi
      OUT_DIR="$1"
      shift
      ;;
  esac
done

if [[ "$PROFILE" != "min" && "$PROFILE" != "service" ]]; then
  echo "[unitlab] ERROR: unsupported profile '$PROFILE' (expected min|service)" >&2
  usage
  exit 1
fi

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="$ROOT_DIR/dist-release/unitlab-core-rpi-runtime-${PROFILE}-${RELEASE_VERSION}"
fi

command -v rsync >/dev/null 2>&1 || {
  echo "[unitlab] ERROR: rsync not found" >&2
  exit 1
}

command -v python3 >/dev/null 2>&1 || {
  echo "[unitlab] ERROR: python3 not found (required for host-agent wheel build)" >&2
  exit 1
}

echo "[unitlab] Creating RPi runtime bundle at: $OUT_DIR (profile=$PROFILE)"
[[ -n "$OUT_DIR" && "$OUT_DIR" != "/" ]] || {
  echo "[unitlab] ERROR: invalid OUT_DIR" >&2
  exit 1
}
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

copy() {
  local src="$1"
  local dst="$2"
  mkdir -p "$(dirname "$dst")"
  rsync -a "$src" "$dst"
}

required_file() {
  local path="$1"
  [[ -f "$path" ]] || {
    echo "[unitlab] ERROR: missing required file: $path" >&2
    exit 1
  }
}

required_dir() {
  local path="$1"
  [[ -d "$path" ]] || {
    echo "[unitlab] ERROR: missing required directory: $path" >&2
    exit 1
  }
}

build_host_agent_wheelhouse() {
  local wheelhouse_dir="$1"
  local py_bin
  local wheel_mode=""
  local agents=(
    rpi-net-agent
    rpi-ntp-agent
    rpi-core-diag-agent
    rpi-provision-agent
  )

  resolve_python_for_wheels() {
    local candidates=()
    local candidate

    if [[ -n "${PYTHON_BIN:-}" ]]; then
      candidates+=("$PYTHON_BIN")
    fi
    candidates+=("python3" "/usr/bin/python3")

    for candidate in "${candidates[@]}"; do
      command -v "$candidate" >/dev/null 2>&1 || continue

      if "$candidate" -m pip --version >/dev/null 2>&1; then
        echo "$candidate"
        return 0
      fi

      if "$candidate" -m ensurepip --upgrade >/dev/null 2>&1 && "$candidate" -m pip --version >/dev/null 2>&1; then
        echo "$candidate"
        return 0
      fi
    done

    return 1
  }

  if py_bin="$(resolve_python_for_wheels)"; then
    wheel_mode="python-pip"
    echo "[unitlab] Host-agent wheel build python: $py_bin"
  elif command -v uv >/dev/null 2>&1; then
    wheel_mode="uv-pip"
    echo "[unitlab] Host-agent wheel build via uv tool run (pip)"
  else
    echo "[unitlab] ERROR: no usable Python with pip found for wheel build" >&2
    echo "[unitlab] HINT: install pip for system python (e.g. apt install python3-pip), set PYTHON_BIN, or install uv" >&2
    exit 1
  fi

  mkdir -p "$wheelhouse_dir"
  rm -f "$wheelhouse_dir"/*.whl

  for agent in "${agents[@]}"; do
    local agent_dir="$ROOT_DIR/host-services/$agent"
    [[ -d "$agent_dir" ]] || { echo "[unitlab] ERROR: missing host agent dir: $agent_dir" >&2; exit 1; }
    rm -rf "$agent_dir/build" "$agent_dir/dist"
    find "$agent_dir" -maxdepth 1 -type d -name '*.egg-info' -exec rm -rf {} +
    echo "[unitlab] Building wheel for $agent"
    if [[ "$wheel_mode" == "python-pip" ]]; then
      "$py_bin" -m pip wheel --wheel-dir "$wheelhouse_dir" "$agent_dir"
    else
      uv tool run --from pip pip wheel --wheel-dir "$wheelhouse_dir" "$agent_dir"
    fi
  done
}

# Validate required inputs early for clear operator errors
required_file "$ROOT_DIR/docker-compose.prod.yml"
required_dir "$ROOT_DIR/config"
required_dir "$ROOT_DIR/host-services"

# Core runtime manifests/configs
copy "$ROOT_DIR/docker-compose.prod.yml" "$OUT_DIR/"
rsync -a \
  --exclude '*.dev.yml' \
  "$ROOT_DIR/config/" "$OUT_DIR/config/"

# Host services (installed on host via systemd)
rsync -a \
  --exclude '.venv' \
  --exclude '__pycache__' \
  --exclude '*.log' \
  --exclude '.DS_Store' \
  "$ROOT_DIR/host-services/" "$OUT_DIR/host-services/"

# Host-agent wheelhouse (offline install source for installer)
if [[ "$BUILD_HOST_AGENT_WHEELS" == "1" ]]; then
  echo "[unitlab] Building host-agent wheelhouse"
  build_host_agent_wheelhouse "$OUT_DIR/wheels"
else
  echo "[unitlab] WARN: skipping host-agent wheelhouse build (--skip-host-agent-wheels)"
fi

# Core deploy/runtime scripts (required on RPi in all profiles)
mkdir -p "$OUT_DIR/scripts"
copy "$ROOT_DIR/scripts/deploy-rpi.sh" "$OUT_DIR/scripts/"
copy "$ROOT_DIR/scripts/verify-rpi-runtime.sh" "$OUT_DIR/scripts/"
copy "$ROOT_DIR/scripts/cleanup-rpi-releases.sh" "$OUT_DIR/scripts/"
copy "$ROOT_DIR/scripts/install-host-agents.sh" "$OUT_DIR/scripts/"
copy "$ROOT_DIR/scripts/verify-host-agents.sh" "$OUT_DIR/scripts/"
chmod +x \
  "$OUT_DIR/scripts/deploy-rpi.sh" \
  "$OUT_DIR/scripts/verify-rpi-runtime.sh" \
  "$OUT_DIR/scripts/cleanup-rpi-releases.sh" \
  "$OUT_DIR/scripts/install-host-agents.sh" \
  "$OUT_DIR/scripts/verify-host-agents.sh"

# Optional service/debug extras
if [[ "$PROFILE" == "service" ]]; then
  # Ops scripts (including release helpers)
  rsync -a "$ROOT_DIR/scripts/" "$OUT_DIR/scripts/"

  # Docs useful during field deployment
  mkdir -p "$OUT_DIR/docs/guide"
  copy "$ROOT_DIR/docs/guide/rpi5-core-provisioning.md" "$OUT_DIR/docs/guide/"
  copy "$ROOT_DIR/docs/guide/frontend-web-image-release.md" "$OUT_DIR/docs/guide/"
fi

# Shared host env templates (persist across release rollbacks)
mkdir -p "$OUT_DIR/shared"
cat > "$OUT_DIR/shared/backend.env.example" <<'EOF'
APP_ENV=production
DEBUG=false
DEBUG_LEVEL=INFO

POSTGRES_USER=unitlab_pg_user
POSTGRES_PASSWORD=unitlab_pg_password
POSTGRES_HOST=db
POSTGRES_PORT=5432
POSTGRES_DB=unitlab_pg

REDIS_HOST=redis
REDIS_PORT=6379

MQTT_HOST=mosquitto
MQTT_PORT=1883
EOF

cat > "$OUT_DIR/shared/db.env.example" <<'EOF'
POSTGRES_USER=unitlab_pg_user
POSTGRES_PASSWORD=unitlab_pg_password
POSTGRES_DB=unitlab_pg
POSTGRES_HOST_AUTH_METHOD=md5
EOF

cat > "$OUT_DIR/.env.example" <<'EOF'
# Root compose env (same directory as docker-compose.prod.yml)
RELEASE_VERSION=2026.02.24-1345
# Backend/runtime app image (api + workers + migrations)
UNITLAB_BACKEND_IMAGE=unitlab-backend:${RELEASE_VERSION}
# Frontend web runtime image (nginx + built dist)
UNITLAB_WEB_IMAGE=unitlab-web:${RELEASE_VERSION}
EOF

cat > "$OUT_DIR/.env.release" <<EOF
RELEASE_VERSION=$RELEASE_VERSION
UNITLAB_BACKEND_IMAGE=$UNITLAB_BACKEND_IMAGE
UNITLAB_WEB_IMAGE=$UNITLAB_WEB_IMAGE
EOF

GIT_SHA="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)"
cat > "$OUT_DIR/RELEASE_INFO" <<EOF
PROFILE=$PROFILE
RELEASE_VERSION=$RELEASE_VERSION
BUILD_TIME=$STAMP
GIT_SHA=$GIT_SHA
EOF

cat > "$OUT_DIR/README_DEPLOY.md" <<'EOF'
# UnitLab RPi Runtime Deploy

1. Copy this bundle to `/opt/unitlab/releases/<bundle>` on RPi5.
2. Create shared env files once (persist across releases):
  ```bash
  sudo mkdir -p /opt/unitlab/shared
  sudo cp -n /opt/unitlab/releases/<bundle>/shared/backend.env.example /opt/unitlab/shared/backend.env
  sudo cp -n /opt/unitlab/releases/<bundle>/shared/db.env.example /opt/unitlab/shared/db.env
  ```
3. Deploy via script (it updates `/opt/unitlab/current` symlink automatically):
  ```bash
  sudo /opt/unitlab/releases/<bundle>/scripts/deploy-rpi.sh \
    --bundle-dir /opt/unitlab/releases/<bundle>
  ```
4. Install/update host agents from bundled wheels:
  ```bash
  sudo /opt/unitlab/current/scripts/install-host-agents.sh
  sudo /opt/unitlab/current/scripts/verify-host-agents.sh
  ```

Rollback: rerun deploy for previous bundle (script handles symlink switch and stack up).
EOF

echo
echo "[unitlab] Bundle created."
echo "[unitlab] Note: frontend source is intentionally omitted (image-based frontend delivery)."
if [[ "$PROFILE" == "service" ]]; then
  echo "[unitlab] Includes service/debug extras (docs + scripts)."
fi
echo "[unitlab] Next (recommended on RPi5):"
echo "[unitlab]   1) Copy bundle to: /opt/unitlab/releases/<bundle>"
echo "[unitlab]   2) Deploy: sudo /opt/unitlab/releases/<bundle>/scripts/deploy-rpi.sh --bundle-dir /opt/unitlab/releases/<bundle>"
echo "[unitlab]   3) deploy-rpi.sh updates /opt/unitlab/current automatically"
echo "[unitlab] Rollback: deploy previous bundle path with deploy-rpi.sh"
