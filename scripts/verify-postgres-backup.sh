#!/usr/bin/env bash
set -euo pipefail

BACKUP_DIR="${BACKUP_DIR:-/var/lib/unitlab/backups/postgres}"
BACKUP_MAX_AGE_HOURS="${BACKUP_MAX_AGE_HOURS:-48}"

log() { echo "[unitlab-backup-check] $*"; }
err() { echo "[unitlab-backup-check] ERROR: $*" >&2; }

[[ "$BACKUP_MAX_AGE_HOURS" =~ ^[0-9]+$ ]] || { err "BACKUP_MAX_AGE_HOURS must be an integer"; exit 2; }

latest="$(find "$BACKUP_DIR" -maxdepth 1 -type f -name 'unitlab-*.dump' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n1 | cut -d' ' -f2- || true)"
[[ -n "$latest" && -f "$latest" ]] || { err "no PostgreSQL backup found in $BACKUP_DIR"; exit 1; }
[[ -f "${latest}.sha256" ]] || { err "checksum is missing for $(basename "$latest")"; exit 1; }

(cd "$(dirname "$latest")" && sha256sum --check "$(basename "$latest").sha256")
pg_restore --list "$latest" >/dev/null
latest_epoch="$(stat -c '%Y' "$latest")"
age_seconds=$(( $(date +%s) - latest_epoch ))
max_age_seconds=$((BACKUP_MAX_AGE_HOURS * 3600))
(( age_seconds <= max_age_seconds )) || {
  err "latest backup is too old: $((age_seconds / 3600))h; maximum is ${BACKUP_MAX_AGE_HOURS}h"
  exit 1
}
log "latest backup is readable and checksum-valid: $(basename "$latest")"

if [[ "${VERIFY_RESTORE_DATABASE:-}" == "1" ]]; then
  : "${PGHOST:?PGHOST is required for restore verification}"
  : "${PGUSER:?PGUSER is required for restore verification}"
  : "${PGDATABASE:?PGDATABASE is required for restore verification}"
  target="${VERIFY_RESTORE_DB_NAME:-unitlab_restore_check_$(date -u +%Y%m%d%H%M%S)}"
  export PGDATABASE
  created=0
  cleanup() {
    if (( created == 1 )); then
      dropdb --if-exists --maintenance-db="${VERIFY_RESTORE_MAINTENANCE_DB:-postgres}" "$target" >/dev/null || true
    fi
  }
  trap cleanup EXIT
  createdb --maintenance-db="${VERIFY_RESTORE_MAINTENANCE_DB:-postgres}" "$target"
  created=1
  pg_restore --no-owner --no-acl --dbname="$target" "$latest"
  log "restore verification passed in temporary database: $target"
fi
