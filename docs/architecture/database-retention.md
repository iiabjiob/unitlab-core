# Database retention

The backend runs a database retention worker when `database_retention_enabled` is enabled. The worker wakes at `database_retention_interval_seconds`, uses a PostgreSQL advisory lock so only one API instance performs a run, and deletes records in bounded batches.

The initial policy covers only operational records:

- `processed_jobs`: 30 days by default;
- `core_diagnostics_acknowledgements`: 90 days by default;
- completed or otherwise terminal `hardware_command_intents`: 365 days by default, including their cascade-owned channel rows.
- IEC 61850 runtime selection events: 90 days by default;
- signal allocation events: 730 days by default;
- unreferenced SCL imports: 180 days by default;
- archived, unreferenced signal-list revisions: 730 days by default;
- old SLD document revisions: 180 days by default, while retaining the latest 50 revisions per document.
- completed FAT evidence and sequence runs: 2555 days by default (seven years).

The policy does not delete active or referenced signal-list revisions, allocations, reports, current SLD documents, or current device/channel state. Completed evidence is retained for seven years by default; deployments requiring longer traceability must increase `database_test_evidence_retention_days` or archive evidence before expiry.

Set `database_retention_dry_run=true` to count eligible records without deleting them. Retention failures are logged and do not stop the API process; the next scheduled run retries the cleanup.

The worker also logs database size, filesystem free space, and the largest PostgreSQL tables. A regular `VACUUM (ANALYZE)` runs on the configured interval.

Production Compose runs a separate `db_backup` service. It creates a PostgreSQL custom-format dump every 24 hours by default, validates it with `pg_restore --list`, writes a SHA-256 sidecar, and publishes it atomically into the `db_backups` volume. Archives older than 30 days are removed only after a new archive has been validated. `BACKUP_INTERVAL_SECONDS`, `BACKUP_RETENTION_DAYS`, and `BACKUP_MIN_FREE_GB` can be overridden in the deployment environment.

After a successful backup, the service performs a full restore verification into a temporary database once every 30 days by default. The check is isolated from the production database and does not replace the normal backup freshness check.

The operator can run `sudo unitlab backup-check` to validate the newest archive and its age. `sudo unitlab backup-check --restore` additionally restores it into a temporary database and removes that database after the check; this should be run during a low-activity maintenance window, for example monthly. A production restore requires the explicit `restore-postgres.sh --confirm <backup.dump>` command and a maintenance window. The backup volume is separate from the PostgreSQL data volume, but it is still on the same host; deployments that must survive host loss need an additional off-host copy.

Deployments create a safety backup before migrations and before the explicit destructive database reset. Successful deployments retain the active and previous release directories and remove only dangling Docker image layers. Docker logs are bounded by the Compose `json-file` limits; no retention job removes test evidence or reports.
