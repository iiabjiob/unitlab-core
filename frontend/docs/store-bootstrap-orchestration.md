# Store Bootstrap Orchestration

This project uses a centralized loader helper:

- `runStoreBootstrap(keyParts, tasks, { mode })`
- Location: `src/composables/useStoreBootstrap.ts`

It provides:

- In-flight deduplication per logical key
- Consistent sequencing for store initialization
- Optional fault-tolerance mode (`settled`)

---

## Mode selection

Use `mode: "strict"` when **all tasks are required** for correct screen behavior.

Use cases:

- route guard must block navigation until data is ready
- editor requires mandatory dependencies before render
- operation where partial state is invalid

Behavior:

- any task rejection fails the whole bootstrap
- caller handles error/redirect

Use `mode: "settled"` when **partial readiness is acceptable**.

Use cases:

- warm-up/probing loaders
- optional side panels/modals/toolbars
- best-effort catalog hydration where retry can happen later

Behavior:

- waits for all tasks to settle
- does not throw due to a single task failure

---

## Key design

Build keys from stable dimensions that define the data scope:

- feature name (`"route-signals"`, `"signals-runtime-catalog"`)
- workspace id (if workspace-scoped)
- entity id (if detail-scoped)

Examples:

- `runStoreBootstrap(["route-sequences", workspaceId], [...], { mode: "strict" })`
- `runStoreBootstrap(["signal-selection-grid", workspaceId], [...], { mode: "settled" })`

Avoid:

- random values in keys
- overly broad global keys for workspace-scoped data

---

## Practical rules

1. Prefer orchestrator over direct `ensureLoaded()` in components/guards.
2. Keep tasks idempotent (safe to call multiple times).
3. Use one orchestrator call per logical bootstrap unit.
4. Use `strict` in route guards by default.
5. Use `settled` for optional UX enhancements and non-blocking refresh.

---

## Current adoption points

- Runtime boot: `src/boot/runtime.ts`
- Route guards: devices/sequences/switchgears/signals
- Signals allocation runtime hydration
- Channel tree picker catalog load
- Switchgear bindings/toolbar helper loads
- Signal selection modal refresh

---

## Notes

The orchestrator wraps task execution in a microtask (`Promise.resolve().then(...)`) before storing in-flight promises. This avoids sync-throw timing gaps and makes deduplication race-safe.
