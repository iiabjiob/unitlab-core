#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
COMPOSE_FILE="${COMPOSE_FILE:-docker-compose.prod.yml}"
COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-unitlab}"

if [[ "${RESET_DATABASE_CONFIRM:-}" != "YES" ]]; then
  echo "[unitlab] ERROR: set RESET_DATABASE_CONFIRM=YES to delete database volumes" >&2
  exit 1
fi

if [[ "$PROJECT_DIR" == "/" || ! -d "$PROJECT_DIR" ]]; then
  echo "[unitlab] ERROR: invalid PROJECT_DIR: $PROJECT_DIR" >&2
  exit 1
fi

command -v docker >/dev/null 2>&1 || {
  echo "[unitlab] ERROR: docker is required" >&2
  exit 1
}

compose=(
  docker compose
  --project-name "$COMPOSE_PROJECT_NAME"
  --project-directory "$PROJECT_DIR"
  -f "$PROJECT_DIR/$COMPOSE_FILE"
)

echo "[unitlab] Removing PostgreSQL and Redis data volumes for project: $COMPOSE_PROJECT_NAME"
echo "[unitlab] Creating safety backup before destructive reset"
"${compose[@]}" up -d db
db_ready=0
for _ in $(seq 1 30); do
  if "${compose[@]}" exec -T db pg_isready >/dev/null 2>&1; then
    db_ready=1
    break
  fi
  sleep 2
done
if (( db_ready == 0 )); then
  echo "[unitlab] ERROR: PostgreSQL did not become ready for safety backup" >&2
  exit 1
fi
"${compose[@]}" run --rm --no-deps db_backup /usr/local/bin/backup-postgres.sh
"${compose[@]}" down --remove-orphans
docker volume rm \
  "${COMPOSE_PROJECT_NAME}_db_data" \
  "${COMPOSE_PROJECT_NAME}_redis_data" \
  2>/dev/null || true
echo "[unitlab] Recreating schema from baseline migration"
"${compose[@]}" up --force-recreate migrations
echo "[unitlab] Starting application stack"
"${compose[@]}" up -d --remove-orphans
