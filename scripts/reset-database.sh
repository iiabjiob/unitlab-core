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

echo "[unitlab] Removing compose volumes for project: $COMPOSE_PROJECT_NAME"
"${compose[@]}" down --volumes --remove-orphans
echo "[unitlab] Recreating schema from baseline migration"
"${compose[@]}" up --force-recreate migrations
echo "[unitlab] Starting application stack"
"${compose[@]}" up -d --remove-orphans
