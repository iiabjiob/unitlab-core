# Database retention

The backend runs a database retention worker when `database_retention_enabled` is enabled. The worker wakes at `database_retention_interval_seconds`, uses a PostgreSQL advisory lock so only one API instance performs a run, and deletes records in bounded batches.

The initial policy covers only operational records:

- `processed_jobs`: 30 days by default;
- `core_diagnostics_acknowledgements`: 90 days by default;
- completed or otherwise terminal `hardware_command_intents`: 365 days by default, including their cascade-owned channel rows.

The policy does not delete signal-list revisions, allocations, reports, test evidence, sequence runs, SLD documents, or current device/channel state. Those records need reference-aware retention and a separate archive policy because they can be required to reconstruct FAT evidence.

Set `database_retention_dry_run=true` to count eligible records without deleting them. Retention failures are logged and do not stop the API process; the next scheduled run retries the cleanup.
