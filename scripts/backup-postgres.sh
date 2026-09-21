#!/usr/bin/env bash
set -euo pipefail

umask 077

BACKUP_DIR="${BACKUP_DIR:-/var/lib/unitlab/backups/postgres}"
BACKUP_RETENTION_DAYS="${BACKUP_RETENTION_DAYS:-30}"
BACKUP_MIN_FREE_GB="${BACKUP_MIN_FREE_GB:-5}"
BACKUP_LOCK_DIR="${BACKUP_LOCK_DIR:-${BACKUP_DIR}/.lock}"
PGHOST="${PGHOST:-${POSTGRES_HOST:-db}}"
PGPORT="${PGPORT:-${POSTGRES_PORT:-5432}}"
PGUSER="${PGUSER:-${POSTGRES_USER:?POSTGRES_USER is required}}"
PGDATABASE="${PGDATABASE:-${POSTGRES_DB:?POSTGRES_DB is required}}"
export PGHOST PGPORT PGUSER PGDATABASE

if [[ -z "${PGPASSWORD:-}" && -n "${POSTGRES_PASSWORD:-}" ]]; then
  export PGPASSWORD="$POSTGRES_PASSWORD"
fi

log() { echo "[unitlab-backup] $*"; }
err() { echo "[unitlab-backup] ERROR: $*" >&2; }

is_positive_integer() {
  [[ "$1" =~ ^[0-9]+$ ]] && ((10#$1 > 0))
}

is_positive_integer "$BACKUP_RETENTION_DAYS" || { err "BACKUP_RETENTION_DAYS must be a positive integer"; exit 2; }
is_positive_integer "$BACKUP_MIN_FREE_GB" || { err "BACKUP_MIN_FREE_GB must be a positive integer"; exit 2; }

mkdir -p "$BACKUP_DIR"
if ! mkdir "$BACKUP_LOCK_DIR" 2>/dev/null; then
  log "another backup is already running; skipping"
  exit 0
fi
trap 'rmdir "$BACKUP_LOCK_DIR" 2>/dev/null || true' EXIT

free_kb="$(df -Pk "$BACKUP_DIR" | awk 'NR==2 {print $4}')"
required_kb=$((10#$BACKUP_MIN_FREE_GB * 1024 * 1024))
[[ "$free_kb" =~ ^[0-9]+$ ]] || { err "cannot determine free space for $BACKUP_DIR"; exit 1; }
(( free_kb >= required_kb )) || {
  err "only $((free_kb / 1024 / 1024)) GiB free for backups; minimum is ${BACKUP_MIN_FREE_GB} GiB"
  exit 1
}

timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
archive="$BACKUP_DIR/unitlab-${timestamp}.dump"
temporary="${archive}.tmp"
metadata="${archive%.dump}.meta"
checksum="${archive}.sha256"
cleanup_temporary() { rm -f -- "$temporary"; }
trap 'cleanup_temporary; rmdir "$BACKUP_LOCK_DIR" 2>/dev/null || true' EXIT

log "creating PostgreSQL backup: database=$PGDATABASE host=$PGHOST"
pg_dump --format=custom --no-owner --no-acl --file="$temporary" "$PGDATABASE"
pg_restore --list "$temporary" >/dev/null
mv -- "$temporary" "$archive"
sha256sum "$archive" > "$checksum"
cat > "$metadata" <<EOF
created_at=$timestamp
database=$PGDATABASE
host=$PGHOST
format=postgres-custom
EOF

find "$BACKUP_DIR" -maxdepth 1 -type f -name 'unitlab-*.dump' -mtime "+$BACKUP_RETENTION_DAYS" -print0 |
  while IFS= read -r -d '' old_archive; do
    old_checksum="${old_archive}.sha256"
    old_metadata="${old_archive%.dump}.meta"
    rm -f -- "$old_archive" "$old_checksum" "$old_metadata"
  done

sync
log "backup ready: $(basename "$archive")"
