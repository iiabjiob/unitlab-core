# Interface

## Purpose

Use this page as the navigation map for daily work across Home, Signals, Switchgears, and Instructions.

## Main sections

- **Home**: status overview and quick entry points.
- **Signals**: import signal list, allocate channels, run quick tests.
- **Switchgears**: bind command/feedback and send pair commands.
- **Instructions**: build and run sequence-based procedures.

## Steps

### Typical daily flow

1. Open **Home** and confirm system/workspace readiness.
2. Go to **Signals** and import or verify signal allocation.
3. Go to **Switchgears** and verify binding + control state.
4. Go to **Instructions** and run repeatable procedures.

### Home quick actions

- **Import signal list from project** opens Signals import wizard directly.
- **Start controlling signals** opens Signals when a sheet already exists.
- Scenario cards open target modules: Live Hardware, Signals, Switchgears, Sequencer.

## Verify

### How to read statuses

- **Online**: endpoint is reachable.
- **Offline**: endpoint is not reachable.
- **Pending/Cancelling**: command or run is in transition.
- **Error**: operation failed; check message/log.

## Common issues

### Safe operation rules

1. Confirm active workspace first.
2. Confirm online state before control actions.
3. After each critical action, verify state badge and log entry.
4. Use stop/cancel controls instead of refreshing the page during active runs.

## Next

- [Getting Started](/guide/quick-start)
- [Signals](/guide/signals)
- [Switchgear](/guide/switchgears)
- [Instructions (Sequences)](/guide/sequences)
