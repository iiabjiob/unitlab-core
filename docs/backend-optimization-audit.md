# Backend Optimization Audit & Implementation Pipeline

## Goal
Make the backend predictable, fast, and maintainable:
- enforce clear transaction boundaries (no accidental partial commits)
- split oversized modules by responsibility (HTTP / domain / infra / workers)
- reduce hidden global state and import-time side effects
- standardize worker/job processing patterns
- improve observability and regression safety without slowing product delivery

## What Is Already Good
- Async stack is already in place (`FastAPI` + `SQLAlchemy async` + `redis.asyncio`).
- Background processing is separated into dedicated workers (sequence, signal allocation, signal test run, MQTT ingress/outbound).
- Redis streams + consumer groups are a solid base for durable job processing.
- Signal/test-run functionality already has domain tests for key service logic (`backend/tests/services/*`).
- Domain-rich product behavior exists (signal sheet, test runs, switchgear bindings, sequences), which means optimization effort has strong business ROI.

## Current Architecture Snapshot (observed)
- `backend/app/services/sequence_runner.py` is ~1023 LOC (largest backend file).
- `backend/app/api/v1/signal_sheet/repository.py` is ~869 LOC.
- `backend/app/api/v1/signal_sheet/router.py` is ~649 LOC.
- `backend/tests` currently has 4 test files (mostly service-level helper coverage).
- `get_settings()` is instantiated in many modules (`~23` call sites) instead of a cached singleton.

## Current Architectural Leaks (Highest Impact)

### 1. Config and DB setup have import-time side effects and unsafe defaults
- `backend/app/core/config.py`
  - `print("ENV_FILE resolved to:", ENV_FILE)` at import time.
  - `get_settings()` returns a new `Settings()` instance every call (no cache).
- `backend/app/infrastructure/db/database.py`
  - `create_async_engine(..., echo=True)` is hardcoded.

Risks:
- noisy logs and accidental stdout pollution
- repeated settings parsing
- SQL logging overhead and possible sensitive SQL/value leakage in production

### 2. Transaction boundaries are inconsistent (router + repository both commit)
Examples:
- `backend/app/api/v1/signal_sheet/router.py` commits directly (`await db.commit()`)
- many repository methods commit internally across `api/v1/*/repository.py`

Risks:
- partial writes if one layer commits before a larger operation completes
- harder retries/rollback semantics
- hard-to-reason behavior in workers and chained service calls

### 3. Business/domain logic lives inside `api/v1/.../repository.py` modules
The strongest example:
- `backend/app/api/v1/signal_sheet/repository.py` includes:
  - DB access
  - allocation rules
  - auto-allocation policy
  - row building and domain projection
  - progress callbacks

Risks:
- API layer paths become de facto domain layer
- difficult test boundaries
- high blast radius for changes (DB/query change can break domain behavior and vice versa)

### 4. Large “god files” mix orchestration + IO + domain logic
Main hotspots:
- `backend/app/services/sequence_runner.py` (~1023 LOC)
- `backend/app/api/v1/signal_sheet/repository.py` (~869 LOC)
- `backend/app/api/v1/signal_sheet/router.py` (~649 LOC)
- `backend/app/workers/signal_test_run_runner.py` (~492 LOC)

Risks:
- regressions during feature changes
- low discoverability for engineers
- duplicated patterns (especially worker loops and progress handling)

### 5. Worker consumer-loop boilerplate is duplicated
Patterns repeated across workers:
- `_ensure_group`
- `_fetch`
- `_drain_pending`
- stream replay / xack / error handling
- stop signal handling / heartbeat lifecycle

Files:
- `backend/app/workers/sequence_runner.py`
- `backend/app/workers/signal_allocation_runner.py`
- `backend/app/workers/signal_test_run_runner.py`
- likely similar in MQTT worker paths

Risks:
- fixes land in one worker but not others
- inconsistent retry/ack semantics
- maintenance cost grows with each new worker

### 6. WebSocket broadcast path is simplistic and can become a bottleneck
Files:
- `backend/app/ws/manager.py`
- `backend/app/ws/pubsub_listener.py`

Observations:
- global singleton `WebSocketManager`
- in-memory connection list
- serial `broadcast()` loop
- no explicit backpressure strategy / per-client send timeout / queue

Risks:
- one slow/broken client can delay broadcast loop
- hidden latency spikes under many clients
- difficult testing due to global mutable singleton state

### 7. Layering is blurred between API, services, repositories, and workers
Symptoms:
- “repository” modules under `api/v1/*` perform domain decisions and commits
- routers sometimes orchestrate transactional behavior
- workers directly compose repo + service + event publishing logic in one file
- side-effect registration by import:
  - `backend/app/main.py` imports MQTT handlers for registration side effects

Risks:
- coupling between transport layer and domain layer
- difficult refactors and harder onboarding

### 8. Observability and guardrails are still thin for backend runtime hotspots
What is missing (or not standardized):
- request/job correlation IDs across HTTP -> Redis stream -> worker -> WS event
- timing metrics for repository queries and worker job stages
- counters for stream backlog / pending replay / retries / DLQ
- integration tests around worker processing and API transaction flows

Risks:
- slowdowns are hard to localize (DB vs Redis vs worker vs WS)
- regressions show up late in manual QA

## Optimization Principles (Backend Rules)
1. Transaction ownership is explicit.
- One layer owns `commit/rollback` per use case (service/application layer preferred).

2. Repositories do data access and persistence primitives only.
- No domain orchestration, no transport concerns.

3. API routers stay thin.
- Validate request, call application service, map response/error.

4. Workers use a shared processing framework.
- Stream read/replay/ack/retry/heartbeat lifecycle should be standardized.

5. Settings and infrastructure clients are cached and side-effect free at import.
- No `print`, no debug SQL by default, no hidden initialization.

6. Observability is built in, not retrofitted after incidents.
- timers, counters, correlation IDs, backlog visibility

7. Worker idempotency strategy must match job shape.
- Short atomic DB jobs:
  use atomic acquire (`INSERT ... ON CONFLICT DO NOTHING RETURNING 1`) inside the same DB transaction as domain writes.
- Long-running jobs (sleep/IO/device commands):
  do not pretend a single DB transaction can cover the whole execution.
  Use lease/attempt protocol + terminal completion marker (best-effort processed marker alone is not enough).

## Correct Backend Responsibility Model (Target)

### HTTP/API Layer
Purpose: transport only
- request validation
- auth/workspace checks
- call application service
- map domain errors -> HTTP errors

### Application Services Layer
Purpose: use-case orchestration and transaction boundary
- begin/commit/rollback unit of work
- coordinate repositories
- publish events / enqueue jobs
- enforce business workflow invariants

### Domain Logic Layer
Purpose: deterministic rules
- allocation policies
- switchgear binding interpretation
- sequence step semantics
- validation of pair/signal bindings

### Infrastructure Layer
Purpose: side effects and adapters
- DB repositories
- Redis stream bus
- MQTT adapters
- WS publisher

### Worker Runtime Layer
Purpose: generic job runner shell
- group setup
- fetch/replay/ack/retry
- heartbeat and graceful shutdown
- calls application service handlers for job payloads

## Phased Pipeline

### P0. Config / Infra Hygiene (quick wins, low risk)
1. Cache settings with `@lru_cache` in `backend/app/core/config.py`.
2. Remove import-time `print(...)` in config.
3. Make SQLAlchemy `echo` configurable (`settings.debug` / explicit `db_echo` setting), default `False`.
4. Add engine pool settings explicitly (pool size / overflow / ping) if not already tuned externally.
5. Standardize logger initialization at module level without side effects.

Success criteria:
- no import-time stdout noise
- settings object created once per process
- SQL logging disabled by default in prod/dev unless explicitly enabled

### P1. Transaction Boundary Normalization (highest ROI)
1. Decide one transaction owner per path (recommended: application service / use-case service).
2. Refactor repositories to stop calling `commit()` (allow `flush()` where needed).
3. Move `commit/rollback` to service layer for complex operations:
- signal sheet import
- allocation bulk updates
- switchgear create/update binding flows
- sequence create/update step batches
4. Add a lightweight Unit-of-Work helper around `AsyncSession`.
5. Standardize error handling so failed workflows rollback fully.

Success criteria:
- no mixed commit ownership (router + repo)
- multi-step workflows commit atomically
- easier retries and integration tests

### P2. Layering Cleanup (API vs Application vs Repository)
1. Move domain-heavy repository logic out of `api/v1/*/repository.py`.
2. Create application/use-case modules for:
- signal sheet import / preview / presets
- signal allocation jobs orchestration
- switchgear binding/auto-binding
- sequence command orchestration
3. Keep routers thin and declarative.
4. Rename modules to reflect true roles (`repositories`, `use_cases`, `services`, `workers`).

Priority targets:
- `backend/app/api/v1/signal_sheet/repository.py`
- `backend/app/api/v1/signal_sheet/router.py`
- `backend/app/services/sequence_runner.py`

Success criteria:
- API routers mostly map request/response
- repositories primarily contain queries and persistence
- domain rules are unit-testable without HTTP stack

### P3. Worker Runtime Framework (reduce duplication, improve reliability)
1. Extract shared Redis stream consumer loop:
- ensure group
- fetch batch
- replay pending
- ack policy
- retry / DLQ hook
- graceful shutdown and heartbeat
2. Use one worker shell implementation with pluggable handlers for:
- sequence commands
- signal allocation jobs
- signal test run jobs
3. Standardize retry strategy and DLQ behavior (per worker type config).
4. Add worker health/backlog metrics (pending replay count, retries, failed jobs).

Success criteria:
- less duplicated worker boilerplate
- consistent ack/retry semantics across workers
- faster bug fixes across all workers

### P4. Signal Sheet / Allocation Domain Split (backend hotspot)
1. Split `signal_sheet` backend into modules by responsibility:
- import parsing / metadata preset logic
- allocation query projection (rows/paging/by ids)
- allocation mutation / auto-allocate policy
- test-run support (`mark_signals_tested_at`, ensure allocated, report export helpers)
2. Move heavy row-building/query code to repository/query module only.
3. Move auto-allocation policy and channel matching to pure domain service/helper.
4. Add targeted tests for allocation policy (online-first, single-unit preference, stable channel ordering).

Success criteria:
- signal sheet changes don’t require editing one 800+ LOC file
- allocation rules tested independent of DB plumbing

### P5. WebSocket/Event Delivery Hardening
1. Replace direct serial broadcast with per-client send isolation:
- timeout or bounded queue per websocket
- drop/cleanup slow clients deterministically
2. Keep `WebSocketManager` transport-only; move state-sync orchestration elsewhere.
3. Add structured event envelopes with correlation IDs (job_id, request_id).
4. Add counters/timings for WS publish/broadcast path and dropped clients.

Success criteria:
- slow client does not degrade all WS clients
- easier diagnosis of delayed UI updates

### P6. Observability, Profiling, and Tests (prevent regressions)
1. Add backend metrics/logging timers for:
- HTTP handlers (top endpoints)
- repository query durations (hot paths)
- Redis stream worker cycle time
- job stage durations (queued -> running -> completed)
2. Add correlation IDs across:
- HTTP request
- enqueued job
- worker logs
- WS event
3. Expand tests:
- API integration tests for critical flows (`signal-sheet import`, `allocations`, `switchgears`)
- worker processing tests (happy path + retry + DLQ)
- transaction rollback tests for multi-step workflows
4. Add smoke checks for startup configuration (settings/env/database/redis readiness behavior).

Success criteria:
- hotspots are measurable
- failures are traceable across components
- refactors are safer

## Worker Idempotency Patterns (Rulebook)

### Pattern A: Atomic Acquire (preferred for short DB-bound jobs)
Use this for jobs like `signal_allocation_runner` (`auto_allocate`, `bulk_update`) where the core work is a bounded DB mutation and can be committed in one transaction.

Required shape:
1. Open DB session/transaction in worker.
2. `try_acquire_processed_job(...)` using `INSERT ... ON CONFLICT DO NOTHING RETURNING 1`.
3. If not acquired:
- recover/repair terminal Redis job-state if needed
- `ACK` stream entry
- do not execute domain logic
4. Run domain logic.
5. Commit once (acquire marker + domain writes atomically).

Why:
- closes check-then-insert race under at-least-once replay
- no duplicate DB mutations after successful commit + missing ACK

### Pattern B: Best-Effort Marker (temporary fallback for long-running jobs)
Use only when a job contains sleeps / device IO / long loops and cannot be wrapped in one DB transaction.

Current state:
- acceptable as an interim replay-reduction mechanism
- not a full idempotency guarantee

Risks:
- marker may be written after partial external side effects
- replay behavior is still protocol-dependent

### Pattern C (Target): Execution Lease + Terminal Completion Marker (for long-running jobs)
Recommended target for long-running workflows (and now partially implemented for `signal_test_run_runner` via Redis execution lease + best-effort terminal marker).

Minimal model:
- `job_execution_attempts` (or `job_leases`) table / Redis lease key with:
  - `job_id`
  - `worker_name`
  - `attempt_id`
  - `status` (`running|completed|failed|cancelled`)
  - `lease_expires_at` / heartbeat
  - `progress_cursor` (optional)
- terminal completion marker persisted durably (DB) before stream `ACK`

Protocol outline:
1. Worker acquires execution lease for `job_id`.
2. If lease already active and fresh -> do not run.
3. Worker heartbeats lease while running.
4. Worker writes progress/terminal completion marker.
5. `ACK` only after terminal marker persisted.
6. Replay checks terminal marker first, then lease status.

This avoids holding one DB transaction for the whole run while still giving deterministic replay semantics.

## Suggested Implementation Order (PR / commit sequence)
1. P0 config/DB hygiene (`get_settings` cache, remove `print`, `echo` config)
2. P1 transaction boundary policy + one pilot flow (`signal-sheet import` + allocations)
3. P2 layering cleanup for `signal_sheet` (router/use-case/repo split)
4. P3 worker runtime base extraction (pilot on `signal_allocation_runner`)
5. P4 signal sheet domain split and tests (online-first allocation policy)
6. P5 WS broadcast hardening
7. P6 observability + integration tests

## Immediate Code Targets
- `backend/app/core/config.py`
- `backend/app/infrastructure/db/database.py`
- `backend/app/main.py`
- `backend/app/api/v1/signal_sheet/router.py`
- `backend/app/api/v1/signal_sheet/repository.py`
- `backend/app/services/signal_job_service.py`
- `backend/app/workers/signal_allocation_runner.py`
- `backend/app/workers/signal_test_run_runner.py`
- `backend/app/workers/sequence_runner.py`
- `backend/app/ws/manager.py`
- `backend/app/ws/pubsub_listener.py`

## Immediate Quick Wins (can be done first without behavior changes)
1. Cache `get_settings()` and remove `print(...)` in config.
2. Disable hardcoded `echo=True` by default.
3. Add a small shared worker utility for `_ensure_group/_fetch` to stop new duplication.
4. Add lightweight timing logs/counters around signal allocation and test-run worker loops.

## Notes on Repositories and Commits (important)
Not every `commit()` in a repository is wrong in isolation.
It becomes a problem when:
- the same use case spans multiple repositories/services
- router and repository both commit
- workers need atomic multi-step updates + event publishing

The main improvement is consistency and explicit transaction ownership, not “ban commits everywhere” blindly.

## Current Status (Implemented In This Pass)
Done now:
- P0 (partial): `backend/app/core/config.py`
  - removed import-time `print(...)`
  - `get_settings()` is now cached with `@lru_cache(maxsize=1)`
  - added explicit `db_echo: bool = False` setting
- P0 (partial): `backend/app/infrastructure/db/database.py`
  - SQLAlchemy engine no longer hardcodes `echo=True`; uses `settings.db_echo`
  - enabled `pool_pre_ping=True` for safer pooled connections
- P0 (partial): `backend/app/infrastructure/redis/manager.py`
  - `RedisManager.start()` now fails fast on ping/connect failure (raises after cleanup) instead of logging and leaving app to fail later with uninitialized Redis client
- P1 (pilot, partial): `signal-sheet import` transaction boundary normalized in `backend/app/api/v1/signal_sheet/router.py`
  - import + allocation reset + sheet upsert + optional preset save now commit in one transaction
  - rollback added on mutation failure
- P1 (pilot support): `backend/app/api/v1/signal_sheet/repository.py`
  - `save_preset(...)` / `delete_preset(...)` now support `commit: bool = True` for gradual migration of commit ownership from repositories to orchestration layer
- P1 (expanded, partial): synchronous `signal_sheet` mutation endpoints now own transactions in `backend/app/api/v1/signal_sheet/router.py`
  - `presets` save/delete
  - `signal-allocations` bulk update
  - `signal-allocations/auto`
  - `signal-allocations/ensure`
  - `signal-allocations/tested`
- P1 (expanded support): `backend/app/api/v1/signal_sheet/repository.py`
  - `update_allocations(...)`, `auto_allocate(...)`, `mark_signals_tested(...)`, `mark_signals_tested_at(...)` now support `commit: bool = True`
- P2 (started, partial): added application service `backend/app/services/signal_sheet_write_service.py`
  - owns transaction orchestration for synchronous `signal_sheet` mutation flows (`import`, presets save/delete, allocations update/auto, mark tested)
- P2 (started, partial): `backend/app/api/v1/signal_sheet/router.py` now delegates write orchestration to `SignalSheetWriteService` while keeping HTTP validation / response mapping
- P2 (continued, partial): extracted signal allocation policy/helpers from `backend/app/api/v1/signal_sheet/repository.py` into `backend/app/api/v1/signal_sheet/allocation_policy.py`
  - channel compatibility checks
  - online/offline unit evaluation
  - tested-at parsing helper
  - auto-allocation candidate pick/sort/preferred-unit logic
- P3 (started, partial): added shared worker runtime utility `backend/app/workers/stream_worker_runtime.py`
  - shared consumer name builder
  - shared consumer-group creation helper
  - shared `xreadgroup` fetch helper
  - shared pending replay/drain helper
- P3 (started, partial): migrated signal workers to shared runtime utility
  - `backend/app/workers/signal_allocation_runner.py`
  - `backend/app/workers/signal_test_run_runner.py`
- P1/P3 (partial): `backend/app/workers/signal_allocation_runner.py` now owns DB transaction explicitly for job execution
  - repository mutation calls use `commit=False`
  - worker performs `session.commit()` on success and `session.rollback()` on exception
- P1/P3 (partial): `backend/app/workers/signal_test_run_runner.py` aligned with explicit transaction ownership pattern
  - worker owns `session.commit()/rollback()`; long-running test-run persists `tested_at` in explicit worker-owned batched commits (`commit=False` in repo + `repo.db.commit()`)
  - terminal job replay guard (`succeeded/failed/cancelled`) and non-mutating payload copy added
  - worker heartbeat starts before pending replay drain
- P1/P3/P6 (partial): added minimal idempotency guard `backend/app/services/processed_job_service.py` for signal workers
  - initially supported worker-startup runtime DDL fallback for `processed_jobs` (now removed from worker startup path after successful Alembic rollout)
  - `signal_allocation_runner` now uses atomic acquire (`INSERT ... ON CONFLICT DO NOTHING RETURNING 1`) at the start of the worker-owned DB transaction, which closes the check-then-insert race under concurrent replay
  - `signal_allocation_runner` recovers terminal Redis job state on duplicate replay and ACKs without re-running domain work
  - `signal_test_run_runner` now uses a Redis execution lease (acquire/refresh/release with owner-check) to prevent concurrent duplicate long-running execution, while still using a best-effort processed marker as terminal replay-reduction fallback
  - full atomic acquire-in-same-transaction is still not practical for `signal_test_run_runner` because the run includes sleeps/IO and cannot hold one DB transaction for the whole run
  - `signal_test_run_runner` also logs in-process lease telemetry (`acquire_busy`, `refresh_lost`, `refresh_error`, `release_*`) for operational debugging
  - `signal_test_run_runner` now writes a best-effort Redis progress cursor/checkpoint (`job cursor` key) during execution; `get_signal_job()` overlays this cursor for runtime inspection and future resume policy work
  - minimal resume policy added for `signal_test_run_runner` via `resume_from_cursor` + `resume_job_id` (replay/restart can continue from a previous job's `progress_cursor.index` with global progress preserved)
  - progress/result payloads now include explicit resume metadata (`resume_applied`, `resume_offset`, `cursor_reason`) for deterministic UI/debug handling
  - `signal_test_run_runner` now assigns explicit execution `attempt_id` / `attempt_no` per run attempt and persists them in running/result/cursor payloads for deterministic crash recovery diagnostics
  - explicit long-running replay policy added: if a pending stream entry is replayed after a started attempt (cursor/attempt evidence exists), the worker does not silently re-run device control; it marks the job failed with a recovery hint and requires an explicit new `resume_from_cursor` job
  - terminal updates in `signal_test_run_runner` are now attempt-scoped (worker checks current job attempt metadata before writing terminal `succeeded/failed/cancelled` state) to avoid stale-attempt overwrite after lease transitions
- P1/P3/P6 (continued): added SQLAlchemy model + Alembic migration for `processed_jobs`
  - model: `backend/app/models/processed_job.py`
  - migration: `backend/alembic/versions/c9a8b7d6e5f4_add_processed_jobs_idempotency_table.py`
  - worker startup no longer calls runtime `ensure_processed_jobs_table()` after migration rollout; schema ownership is Alembic-only
  - runtime DDL fallback code was removed from `backend/app/services/processed_job_service.py` after rollout validation
- P3 (continued, partial): migrated `backend/app/workers/sequence_runner.py` to shared stream worker runtime utility for consumer-name/group/fetch/pending-replay boilerplate
- P3 (continued, partial): added shared worker lifecycle utility `backend/app/workers/worker_lifecycle.py`
  - stop signal handler installation
  - common consume loop (`while not stop_event`)
- P3 (continued, partial): migrated all three Redis-stream workers to shared lifecycle helper
  - `sequence_runner`
  - `signal_allocation_runner`
  - `signal_test_run_runner`
- P0/P4 (runtime presence cleanup, partial): device heartbeat hot path no longer writes `devices.last_seen_at` to Postgres on every heartbeat
  - `backend/app/infrastructure/mqtt/handlers/device_heartbeat.py` now updates Redis presence only (`device:{unit}:last_seen`, `device:{unit}:status`)
  - removes `UPDATE devices ... COMMIT` from MQTT heartbeat handling path
- P0/P4 (runtime presence cleanup, continued): removed unused legacy DB heartbeat helper `DeviceService.touch_last_seen()` from `backend/app/services/device_service.py`
- P6 (partial): added heartbeat substep latency telemetry (`[HBRT]`) in `backend/app/infrastructure/mqtt/handlers/device_heartbeat.py`
  - logs Redis touch / `sadd` / status-get / optional status-set / WS publish / scan-enqueue / total latency (DEBUG)
- P5 (partial): hardened WebSocket broadcast path in `backend/app/ws/manager.py`
  - configurable per-client send timeout (`settings.ws_send_timeout_ms`, default 1500 ms)
  - parallel broadcast (`asyncio.gather`) instead of serial per-client loop
  - auto-disconnect on timeout/send errors (slow/broken client isolation)
  - aggregated broadcast latency debug log (channel/client count/sent/failed/total)
- P5 (continued, partial): added bounded per-client outbound queues + sender loops in `backend/app/ws/manager.py`
  - configurable queue size (`settings.ws_outbound_queue_size`, default 2048)
  - `send_event` / `broadcast` enqueue instead of direct `send_json`
  - slow clients are isolated by queue-full policy + sender-loop timeout disconnect
- P5 (continued, partial): `WebSocketManager.connect()` no longer blocks on `WsStateService.sync_client()`
  - initial sync is started as a background task after accept + sender-loop setup
- P6 (partial): added in-process WS runtime counters in `backend/app/ws/manager.py`
  - counters for connect/disconnect reasons, broadcast enqueue failures, queue-full disconnects, send timeouts/errors, initial-sync failures
  - `WebSocketManager.get_stats()` returns active connection gauges + counters snapshot for diagnostics
- P6 (partial): added WS runtime health endpoint `GET /api/v1/health/ws`
  - implemented in `backend/app/api/v1/health/router.py`
  - exposes `WebSocketManager.get_stats()` snapshot without changing existing `/api/v1/health` response schema
- P6 (partial): added signal test-run lease telemetry endpoints in `backend/app/api/v1/health/router.py`
  - `GET /api/v1/health/signal-test-run/lease`
  - `POST /api/v1/health/signal-test-run/lease/reset`
  - counters are persisted by `signal_test_run_runner` to Redis, so they are visible cross-process from the API service
- P6 (partial): added `POST /api/v1/health/ws/reset` to clear in-process WS counters between profiling scenarios
  - implemented via `WebSocketManager.reset_stats()` in `backend/app/ws/manager.py`
- P5/P6 (partial): added lightweight WS Pub/Sub coalescing for noisy `devices/state` events in `backend/app/ws/pubsub_listener.py`
  - consecutive state events are coalesced by `(unit_id, mode)` and flushed on non-state boundary / threshold / shutdown
  - reduces redundant WS broadcasts during state floods without changing event schema
- P2/P4 (live presence overlay, partial): added Redis-backed presence service `backend/app/services/device_presence_service.py`
  - batch Redis pipeline lookup for `status` + `last_seen`
  - fail-soft fallback (offline map) if Redis lookup errors
- P2/P4 (live presence overlay, partial): `backend/app/services/device_service.py` now overlays `status/last_seen` from Redis for `list/get/update/register` responses
- P2/P4 (live presence overlay, partial): `backend/app/api/v1/signal_sheet/repository.py` now overlays `unit_online/unit_last_seen_at` from Redis and uses Redis presence for auto-allocation online-first sorting/picking

Not closed yet (next implementation passes):
- P0 (full): config/infra hygiene completion across remaining startup modules
- P1 transaction boundary normalization (expand beyond pilot flow)
- P2 layering cleanup (`signal_sheet` split; router->service started, allocation policy extracted, query/mutation repository split still pending)
- P4 signal-sheet/domain cleanup continuation (Redis presence as source of truth for live online/offline is partially implemented; more domain split still pending)
- P3 worker runtime framework extraction (retry/DLQ helpers and full worker shell abstraction still pending; stream + lifecycle baseline now shared)
- P5 WS delivery hardening
- P6 backend observability + integration tests
