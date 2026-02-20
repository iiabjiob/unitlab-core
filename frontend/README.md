# UnitLab Frontend

Vue 3 + Vite frontend for UnitLab UI.

## Setup

```sh
pnpm install
```

## Development

```sh
pnpm dev
```

Optional WebSocket endpoint overrides (useful in dev when Vite proxy drops after sleep/wake):

```sh
VITE_WS_URL=ws://localhost:5173/ws/ws
VITE_WS_FALLBACK_URL=ws://localhost:8000/ws/ws
```

`VITE_WS_FALLBACK_URL` also supports comma-separated values.

## Checks and build

```sh
pnpm type-check
pnpm build
```

## Signals module notes

Signals allocation operations are background job-based:

- auto allocation and bulk allocation/unassign enqueue backend jobs
- progress is received in real time via WebSocket `signal_allocation_job` events
- UI updates when terminal job status is reached

Run test is also backend worker-based and supports right-click configuration:

- `toggle_mode`: `single` or `double`
- `signal_interval_ms` presets via context menu
- default: `single` + `1000 ms`

For detailed product flow, see:

- `docs/guide/signals.md`
- `frontend/docs/signal-workflow.md`
