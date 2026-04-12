#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$PWD}"
COMPOSE_FILE="${COMPOSE_FILE:-$PROJECT_DIR/docker-compose.prod.yml}"
HEALTH_URL="${HEALTH_URL:-http://localhost/api/v1/health}"
MIN_MEM_AVAILABLE_MB="${MIN_MEM_AVAILABLE_MB:-200}"
MAX_DISK_USED_PCT="${MAX_DISK_USED_PCT:-90}"
RESTART_LOOP_THRESHOLD="${RESTART_LOOP_THRESHOLD:-3}"
MAX_IMAGE_COUNT_WARN="${MAX_IMAGE_COUNT_WARN:-30}"
MAX_VOLUME_COUNT_WARN="${MAX_VOLUME_COUNT_WARN:-20}"
HEALTH_STARTUP_GRACE_SEC="${HEALTH_STARTUP_GRACE_SEC:-20}"
HEALTH_POLL_INTERVAL_SEC="${HEALTH_POLL_INTERVAL_SEC:-2}"
RELEASE_VERSION="${RELEASE_VERSION:-}"
REQUIRE_TIME_SYNC="${REQUIRE_TIME_SYNC:-0}"

fail_count=0
warn_count=0

usage() {
  cat <<'EOF'
Usage:
  ./scripts/verify-rpi-runtime.sh [options]

Options:
  --project-dir <path>          Compose project directory (default: current directory)
  --compose-file <path>         Path to docker-compose.prod.yml
  --health-url <url>            Backend/API health endpoint (default: http://localhost/api/v1/health)
  --min-mem-mb <int>            Minimum MemAvailable threshold in MB (default: 200)
  --max-disk-used-pct <int>     Max allowed disk usage percent on / (default: 90)
  --restart-threshold <int>     RestartCount threshold for restart-loop check (default: 3)
  --max-image-count-warn <int>  Warn threshold for local docker images count (default: 30)
  --max-volume-count-warn <int> Warn threshold for docker volumes count (default: 20)
  --health-startup-grace-sec <int>  Grace wait for container health=starting (default: 20)
  --health-poll-interval-sec <int>  Poll interval during health grace wait (default: 2)
  --release-version <value>     Expected release version label (unitlab.release)
  --require-time-sync <0|1>     Fail if chrony is not synchronised (default: 0)
  -h, --help                    Show this help

Environment overrides:
  PROJECT_DIR, COMPOSE_FILE, HEALTH_URL,
  MIN_MEM_AVAILABLE_MB, MAX_DISK_USED_PCT, RESTART_LOOP_THRESHOLD,
  MAX_IMAGE_COUNT_WARN, MAX_VOLUME_COUNT_WARN,
  HEALTH_STARTUP_GRACE_SEC, HEALTH_POLL_INTERVAL_SEC,
  RELEASE_VERSION, REQUIRE_TIME_SYNC
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project-dir)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --project-dir requires a value" >&2; usage; exit 1; }
      PROJECT_DIR="$2"
      shift 2
      ;;
    --compose-file)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --compose-file requires a value" >&2; usage; exit 1; }
      COMPOSE_FILE="$2"
      shift 2
      ;;
    --health-url)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --health-url requires a value" >&2; usage; exit 1; }
      HEALTH_URL="$2"
      shift 2
      ;;
    --min-mem-mb)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --min-mem-mb requires a value" >&2; usage; exit 1; }
      MIN_MEM_AVAILABLE_MB="$2"
      shift 2
      ;;
    --max-disk-used-pct)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --max-disk-used-pct requires a value" >&2; usage; exit 1; }
      MAX_DISK_USED_PCT="$2"
      shift 2
      ;;
    --restart-threshold)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --restart-threshold requires a value" >&2; usage; exit 1; }
      RESTART_LOOP_THRESHOLD="$2"
      shift 2
      ;;
    --max-image-count-warn)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --max-image-count-warn requires a value" >&2; usage; exit 1; }
      MAX_IMAGE_COUNT_WARN="$2"
      shift 2
      ;;
    --max-volume-count-warn)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --max-volume-count-warn requires a value" >&2; usage; exit 1; }
      MAX_VOLUME_COUNT_WARN="$2"
      shift 2
      ;;
    --health-startup-grace-sec)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --health-startup-grace-sec requires a value" >&2; usage; exit 1; }
      HEALTH_STARTUP_GRACE_SEC="$2"
      shift 2
      ;;
    --health-poll-interval-sec)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --health-poll-interval-sec requires a value" >&2; usage; exit 1; }
      HEALTH_POLL_INTERVAL_SEC="$2"
      shift 2
      ;;
    --release-version)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --release-version requires a value" >&2; usage; exit 1; }
      RELEASE_VERSION="$2"
      shift 2
      ;;
    --require-time-sync)
      [[ $# -ge 2 ]] || { echo "[unitlab] ERROR: --require-time-sync requires a value" >&2; usage; exit 1; }
      REQUIRE_TIME_SYNC="$2"
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

ok() {
  echo "[OK] $*"
}

warn() {
  warn_count=$((warn_count + 1))
  echo "[WARN] $*"
}

fail() {
  fail_count=$((fail_count + 1))
  echo "[FAIL] $*"
}

require_cmd() {
  local cmd="$1"
  command -v "$cmd" >/dev/null 2>&1 || {
    echo "[unitlab] ERROR: required command not found: $cmd" >&2
    exit 2
  }
}

container_exists() {
  local name="$1"
  docker inspect "$name" >/dev/null 2>&1
}

container_status() {
  local name="$1"
  docker inspect -f '{{.State.Status}}' "$name" 2>/dev/null || true
}

container_health() {
  local name="$1"
  docker inspect -f '{{if .State.Health}}{{.State.Health.Status}}{{else}}none{{end}}' "$name" 2>/dev/null || true
}

container_restart_count() {
  local name="$1"
  docker inspect -f '{{.RestartCount}}' "$name" 2>/dev/null || echo "0"
}

require_cmd docker
require_cmd curl
require_cmd systemctl

if [[ ! -f "$COMPOSE_FILE" ]]; then
  echo "[unitlab] ERROR: compose file not found: $COMPOSE_FILE" >&2
  exit 2
fi

if ! [[ "$MIN_MEM_AVAILABLE_MB" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --min-mem-mb must be integer" >&2
  exit 2
fi
if ! [[ "$MAX_DISK_USED_PCT" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --max-disk-used-pct must be integer" >&2
  exit 2
fi
if ! [[ "$RESTART_LOOP_THRESHOLD" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --restart-threshold must be integer" >&2
  exit 2
fi
if ! [[ "$MAX_IMAGE_COUNT_WARN" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --max-image-count-warn must be integer" >&2
  exit 2
fi
if ! [[ "$MAX_VOLUME_COUNT_WARN" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --max-volume-count-warn must be integer" >&2
  exit 2
fi
if ! [[ "$HEALTH_STARTUP_GRACE_SEC" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --health-startup-grace-sec must be integer" >&2
  exit 2
fi
if ! [[ "$HEALTH_POLL_INTERVAL_SEC" =~ ^[0-9]+$ ]]; then
  echo "[unitlab] ERROR: --health-poll-interval-sec must be integer" >&2
  exit 2
fi
if [[ "$REQUIRE_TIME_SYNC" != "0" && "$REQUIRE_TIME_SYNC" != "1" ]]; then
  echo "[unitlab] ERROR: --require-time-sync must be 0 or 1" >&2
  exit 2
fi

echo "[unitlab] Verify runtime health"
echo "[unitlab] PROJECT_DIR=$PROJECT_DIR"
echo "[unitlab] COMPOSE_FILE=$COMPOSE_FILE"

echo "[unitlab] --- Host level ---"
if systemctl is-active --quiet chrony; then
  ok "chrony active"
else
  fail "chrony inactive"
fi

if command -v chronyc >/dev/null 2>&1; then
  tracking_output="$(chronyc tracking 2>/dev/null || true)"
  if [[ -n "$tracking_output" ]] && ! grep -qi 'Not synchronised' <<<"$tracking_output"; then
    ok "time synchronised (chronyc tracking)"
  else
    if [[ "$REQUIRE_TIME_SYNC" == "1" ]]; then
      fail "time not synchronised (chronyc tracking)"
    else
      warn "time not synchronised (chronyc tracking); tolerated (set --require-time-sync 1 to fail)"
    fi
  fi
else
  warn "chronyc not installed; sync status check skipped"
fi

if systemctl is-active --quiet docker; then
  ok "docker daemon active"
else
  fail "docker daemon inactive"
fi

if docker info >/dev/null 2>&1; then
  ok "docker info available"
else
  fail "docker info failed"
fi

mem_avail_mb="$(awk '/MemAvailable:/ {printf "%d", $2/1024}' /proc/meminfo 2>/dev/null || echo 0)"
if (( mem_avail_mb >= MIN_MEM_AVAILABLE_MB )); then
  ok "memory available ${mem_avail_mb}MB (threshold ${MIN_MEM_AVAILABLE_MB}MB)"
else
  fail "low memory available ${mem_avail_mb}MB (threshold ${MIN_MEM_AVAILABLE_MB}MB)"
fi

disk_used_pct="$(df -P / | awk 'NR==2 {gsub(/%/, "", $5); print $5}' 2>/dev/null || echo 100)"
if (( disk_used_pct <= MAX_DISK_USED_PCT )); then
  ok "disk usage ${disk_used_pct}% on / (max ${MAX_DISK_USED_PCT}%)"
else
  fail "disk pressure: ${disk_used_pct}% on / (max ${MAX_DISK_USED_PCT}%)"
fi

echo "[unitlab] --- Docker level ---"
if docker compose --project-directory "$PROJECT_DIR" -f "$COMPOSE_FILE" ps >/dev/null 2>&1; then
  ok "docker compose ps available"
else
  fail "docker compose ps failed"
fi

if docker system df >/dev/null 2>&1; then
  ok "docker system df collected"
  image_count="$(docker image ls -q | wc -l | tr -d ' ')"
  volume_count="$(docker volume ls -q | wc -l | tr -d ' ')"
  if (( image_count > MAX_IMAGE_COUNT_WARN )); then
    warn "high docker image count: $image_count (warn threshold ${MAX_IMAGE_COUNT_WARN})"
  else
    ok "docker image count: $image_count"
  fi
  if (( volume_count > MAX_VOLUME_COUNT_WARN )); then
    warn "high docker volume count: $volume_count (warn threshold ${MAX_VOLUME_COUNT_WARN})"
  else
    ok "docker volume count: $volume_count"
  fi
else
  warn "docker system df failed"
fi

readonly expected_running_containers=(
  unitlab-backend
  unitlab-mqtt-ingress
  unitlab-inbound-processor
  unitlab-mqtt-outbound
  unitlab-device-offline
  unitlab-sequence-runner
  unitlab-signal-allocation-runner
  unitlab-signal-test-run-runner
  unitlab-db
  unitlab-nginx
  unitlab-redis
  unitlab-mosquitto
)

for container in "${expected_running_containers[@]}"; do
  if ! container_exists "$container"; then
    fail "$container missing"
    continue
  fi

  status="$(container_status "$container")"
  if [[ "$status" == "running" ]]; then
    ok "$container running"
  else
    fail "$container status=$status"
  fi

  health="$(container_health "$container")"
  if [[ "$health" == "unhealthy" ]]; then
    fail "$container unhealthy"
  fi
  if [[ "$health" == "starting" ]]; then
    if (( HEALTH_STARTUP_GRACE_SEC > 0 && HEALTH_POLL_INTERVAL_SEC > 0 )); then
      waited=0
      while (( waited < HEALTH_STARTUP_GRACE_SEC )); do
        sleep "$HEALTH_POLL_INTERVAL_SEC"
        waited=$((waited + HEALTH_POLL_INTERVAL_SEC))
        health="$(container_health "$container")"
        if [[ "$health" == "healthy" ]]; then
          ok "$container health=healthy after ${waited}s grace"
          break
        fi
        if [[ "$health" == "unhealthy" ]]; then
          fail "$container unhealthy after ${waited}s grace"
          break
        fi
      done
    fi
    if [[ "$health" == "starting" ]]; then
      warn "$container health=starting after ${HEALTH_STARTUP_GRACE_SEC}s grace"
    fi
  fi

  restart_count="$(container_restart_count "$container")"
  if (( restart_count > RESTART_LOOP_THRESHOLD )); then
    fail "$container restart loop suspected (RestartCount=$restart_count)"
  fi

  if [[ -n "$RELEASE_VERSION" ]]; then
    release_label="$(docker inspect -f '{{ index .Config.Labels "unitlab.release" }}' "$container" 2>/dev/null || true)"
    if [[ "$release_label" == "$RELEASE_VERSION" ]]; then
      ok "$container release label matches: $RELEASE_VERSION"
    else
      fail "$container release label mismatch: expected=$RELEASE_VERSION actual=${release_label:-<missing>}"
    fi
  fi
done

if [[ -z "$RELEASE_VERSION" ]]; then
  warn "release label check skipped (set --release-version to enforce unitlab.release)"
fi

echo "[unitlab] --- Service level ---"
health_body="$(curl -fsS "$HEALTH_URL" 2>/dev/null || true)"
if [[ -n "$health_body" ]]; then
  ok "backend health endpoint reachable"
else
  fail "backend health endpoint failed: $HEALTH_URL"
fi

if docker exec unitlab-redis redis-cli ping 2>/dev/null | grep -q '^PONG$'; then
  ok "redis responding (PONG)"
else
  fail "redis not responding"
fi

if docker exec unitlab-db pg_isready -U unitlab_pg_user -d unitlab_pg >/dev/null 2>&1; then
  ok "postgres ready"
else
  fail "postgres not ready"
fi

if command -v nc >/dev/null 2>&1; then
  if nc -z -w 2 127.0.0.1 1883 >/dev/null 2>&1; then
    ok "mosquitto listening on 1883"
  else
    fail "mosquitto port 1883 not reachable"
  fi
elif timeout 2 bash -c 'echo > /dev/tcp/127.0.0.1/1883' >/dev/null 2>&1; then
  ok "mosquitto listening on 1883"
else
  fail "mosquitto port 1883 not reachable"
fi

readonly expected_workers=(
  unitlab-mqtt-ingress
  unitlab-inbound-processor
  unitlab-mqtt-outbound
  unitlab-device-offline
  unitlab-sequence-runner
  unitlab-signal-allocation-runner
  unitlab-signal-test-run-runner
)
worker_ok=true
for worker in "${expected_workers[@]}"; do
  if [[ "$(container_status "$worker")" != "running" ]]; then
    worker_ok=false
    fail "worker not running: $worker"
  fi
done
if [[ "$worker_ok" == true ]]; then
  ok "all ${#expected_workers[@]} workers running"
fi

echo "[unitlab] --- Application level ---"
if [[ -n "$health_body" ]]; then
  if grep -Eqi 'ok|healthy|"status"\s*:\s*"ok"|"status"\s*:\s*"online"' <<<"$health_body"; then
    ok "/api/v1/health status OK"
  else
    warn "/api/v1/health responded but payload has no explicit ok marker"
  fi
else
  fail "/api/v1/health not available"
fi

if container_exists unitlab-migrations; then
  migration_status="$(container_status unitlab-migrations)"
  migration_exit="$(docker inspect -f '{{.State.ExitCode}}' unitlab-migrations 2>/dev/null || echo 1)"
  if [[ "$migration_status" == "exited" && "$migration_exit" == "0" ]]; then
    ok "migrations container completed successfully"
  else
    fail "migrations state=${migration_status:-unknown} exit=${migration_exit}"
  fi
else
  warn "migrations container missing (often expected after stopped-container cleanup)"
fi

echo "---------------------------------"
if (( fail_count == 0 )); then
  echo "UNITLAB RUNTIME STATUS: HEALTHY"
  if (( warn_count > 0 )); then
    echo "[unitlab] warnings: $warn_count"
  fi
  exit 0
fi

echo "UNITLAB RUNTIME STATUS: DEGRADED"
echo "[unitlab] failures: $fail_count, warnings: $warn_count"
exit 1
