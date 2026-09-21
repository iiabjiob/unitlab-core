#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" != "--confirm" ]]; then
  echo "Usage: restore-postgres.sh --confirm <backup.dump>" >&2
  echo "This replaces the target database and must be run during a maintenance window." >&2
  exit 2
fi
shift
archive="${1:-}"
[[ -n "$archive" && -f "$archive" ]] || { echo "[unitlab-restore] ERROR: backup archive is required" >&2; exit 2; }
[[ -f "${archive}.sha256" ]] || { echo "[unitlab-restore] ERROR: backup checksum is missing" >&2; exit 2; }

PGHOST="${PGHOST:-${POSTGRES_HOST:-db}}"
PGPORT="${PGPORT:-${POSTGRES_PORT:-5432}}"
PGUSER="${PGUSER:-${POSTGRES_USER:?POSTGRES_USER is required}}"
PGDATABASE="${PGDATABASE:-${POSTGRES_DB:?POSTGRES_DB is required}}"
export PGHOST PGPORT PGUSER PGDATABASE
if [[ -z "${PGPASSWORD:-}" && -n "${POSTGRES_PASSWORD:-}" ]]; then
  export PGPASSWORD="$POSTGRES_PASSWORD"
fi

(cd "$(dirname "$archive")" && sha256sum --check "$(basename "$archive").sha256")
pg_restore --list "$archive" >/dev/null
echo "[unitlab-restore] restoring $(basename "$archive") into $PGDATABASE"
pg_restore --clean --if-exists --no-owner --no-acl --dbname="$PGDATABASE" "$archive"
echo "[unitlab-restore] restore complete"
