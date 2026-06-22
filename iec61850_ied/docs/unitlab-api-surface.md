# UnitLab API surface

Status: draft service surface for the Python/FastAPI product layer.

This document proposes the minimal HTTP-facing API surface that should sit above the contracts in:
- `docs/unitlab-signal-planning-contract.md`
- `docs/unitlab-runtime-integration-contract.md`
- `docs/unitlab-api-payload-shapes.md`
- `docs/unitlab-verification-execution-contract.md`
- `docs/unitlab-recovery-contract.md`
- `docs/unitlab-diagnostics-contract.md`

The exact route names can still evolve, but the resource boundaries should stay stable.

## Resource groups

### 1. Signal selection and planning

#### `POST /api/v1/verification-targets`

Creates normalized verification targets from selected signal-list rows.

Request:
- `signal_ids` or selected row identifiers;
- optional endpoint / IED context;
- optional SCD/discovery hints;
- optional timeout/window policy.

Response:
- `verification_target_normalization_result`;
- `verification_targets[]`;
- normalization diagnostics;
- source row identity.

#### `POST /api/v1/subscription-plans`

Builds a per-IED / per-RCB subscription plan from verification targets.

Request:
- `verification_targets[]`;
- optional discovery snapshot;
- optional SCD hints.

Response:
- `subscription_plan_result`;
- `subscription_plan`;
- `groups[]`;
- `uncovered_targets[]`;
- planning diagnostics.

### 2. Runtime sessions

#### `GET /api/v1/iec61850/sessions`

Lists current IEC 61850 runtime sessions.

Response:
- `sessions[]` with session snapshots;
- current state and freshness summary;
- diagnostics if present.

#### `POST /api/v1/iec61850/sessions`

Creates or opens a session for an endpoint.

Request:
- `endpoint_id`;
- `ied_name`;
- `access_point_name` if known;
- optional desired planning state.

Response:
- `session_snapshot`;
- connection generation;
- diagnostics.

#### `POST /api/v1/iec61850/sessions/{session_id}/discover`

Triggers discovery for a session.

Response:
- `discovery_snapshot`;
- summary counts;
- diagnostics.

#### `POST /api/v1/iec61850/sessions/{session_id}/subscribe`

Executes the subscription portion of the plan.

Request:
- `subscription_plan` or `group_id`;
- optional `gi_requested`.

Response:
- `selected_report_control`;
- subscription status;
- diagnostics.

#### `POST /api/v1/iec61850/sessions/{session_id}/reconnect`

Requests a recovery/reconnect operation.

Response:
- updated `session_snapshot`;
- recovery state;
- diagnostics.

#### `POST /api/v1/iec61850/sessions/{session_id}/disconnect`

Closes the runtime session cleanly.

Response:
- final `session_snapshot`;
- diagnostics if cleanup was partial.

### 3. Evidence and verdicts

#### `POST /api/v1/test-runs`

Starts a product-level verification run from a plan.

Request:
- `verification_targets[]`;
- `subscription_plan`;
- timing window policy;
- optional operator metadata.

Response:
- `test_run_id`;
- initial state;
- diagnostics.

#### `GET /api/v1/test-runs/{test_run_id}`

Returns current run state.

Response:
- `execution_state`;
- `evidence[]`;
- `verdict`;
- `diagnostics`;
- freshness / stale summary.

#### `POST /api/v1/test-runs/{test_run_id}/abort`

Aborts a running verification flow.

Response:
- final state;
- abort reason;
- diagnostics.

### 4. Runtime update stream

#### `GET /api/v1/iec61850/sessions/{session_id}/events`

Returns runtime events or a server-sent event stream.

Event kinds:
- `session_state_changed`
- `discovery_updated`
- `subscription_updated`
- `report_received`
- `signal_updated`
- `signal_stale`
- `diagnostic_raised`

## API invariants

- Every create/update endpoint should return the latest snapshot plus diagnostics.
- Planning endpoints must be deterministic for the same inputs.
- Runtime endpoints must preserve source identity and generation.
- Evidence endpoints must never erase earlier evidence.
- Event payloads should be append-only in spirit.
- Product verdicts must remain separate from runtime state snapshots.

## Error handling

The API should use structured errors with:
- `code`
- `message`
- `scope`
- `session_id` when relevant
- `endpoint_id` when relevant
- `diagnostic_id` when relevant

Suggested response shape:

```json
{
  "error": {
    "code": "DISCOVERY_FAILED",
    "message": "Discovery could not complete for the selected endpoint.",
    "scope": "discovery",
    "session_id": "sess-12",
    "endpoint_id": "mms:IED1@127.0.0.1:102",
    "diagnostic_id": "diag-44"
  }
}
```

## Suggested implementation modules

- `app/api/verification_targets.py`
- `app/api/subscription_plans.py`
- `app/api/sessions.py`
- `app/api/test_runs.py`
- `app/api/events.py`

## Notes

- The API surface is intentionally thin.
- Business logic should stay in service modules, not route handlers.
- This document does not require any immediate C code change.
