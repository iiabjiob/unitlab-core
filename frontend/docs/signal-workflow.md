# Signal Allocation + Test Run Workflow

## Scope

Current Signals workflow is centered around one allocation editor page:

- import signal list into workspace
- assign/unassign channels
- run background test checks on selected signals
- export cable schedule and report

## Runtime model

Long-running operations do not execute in the browser loop.

- Frontend enqueues a backend job.
- Worker executes operation in background.
- Progress and lifecycle updates are delivered via WebSocket `signal_allocation_job` events.
- UI waits for terminal status (`succeeded`/`failed`) and then refreshes allocations once.

## Job operations

1. `auto_allocate` — background allocation for selected signals.
2. `bulk_update` — background batch updates (including unassign).
3. `test_run` — background run test over selected DO-capable rows.

## Run test behavior

Run test is configurable from right-click context menu on **Run test** button:

- **Toggle mode**:
  - `single` → send ON
  - `double` → send ON, wait interval, send OFF
- **Interval between signals** presets:
  - `500 ms`
  - `1000 ms` (default)
  - `2000 ms`

Defaults:

- `toggle_mode = single`
- `signal_interval_ms = 1000`

Progress in header shows current processed/total, success/skip counters and ETA.

## Timestamp semantics

`Last tested` timestamp is written when a signal command is successfully executed in worker.

- Timestamp is stored per signal at actual completion time.
- This improves precision versus bulk “mark all at end” behavior.

## UX predictability rules

- Only one tooltip can be visible at a time globally.
- While Run test context menu is open, header tooltips are suppressed.
- Right-click on Run test opens settings menu only (no competing tooltip overlay).
