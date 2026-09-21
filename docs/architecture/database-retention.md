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
