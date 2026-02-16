# Getting Started

## What this page is for

Use this quick start to move from login to first safe operations in Signals, Switchgears, and Instructions.

## Step 1 — Open the system and verify connection

1. Open the web interface.
2. Confirm the system status indicator is healthy.
3. If status is degraded/offline, stop and resolve connection issues first.

Expected result:
- Main UI is loaded.
- Status indicator is stable.

## Step 2 — Select workspace

1. Use the workspace switcher in the header.
2. Select the workspace you will operate in.
3. Wait until page counters and data refresh.

Why this matters:
- Signals, Switchgears, and Instructions are workspace-scoped.

## Step 3 — Choose your starting path from Home

From Home, use one of these entry points:
- **Import signal list from project** if you are setting up a workspace.
- **Start controlling signals** if a signal sheet already exists.
- Scenario cards to open modules directly: Live Hardware, Signals, Switchgears, Sequencer.

## Step 4 — Minimum operational readiness check

Before any command:

1. Confirm devices/units needed for operation are online.
2. Confirm required bindings exist (Signals allocations or Switchgear roles).
3. Confirm current state in badges/log before sending a new command.

## Step 5 — First safe execution cycle

1. Perform one small action (single allocation, single switchgear command, or one instruction run).
2. Verify toast result.
3. Verify resulting state in UI badge/grid.
4. Verify corresponding log entry.

Expected result:
- Action succeeds with matching state + log confirmation.

## Next pages

- For signal import/allocation: [Signals](/guide/signals)
- For command control: [Switchgear](/guide/switchgears)
- For repeatable procedures: [Instructions (Sequences)](/guide/sequences)
- For incident handling: [Troubleshooting](/guide/troubleshooting)
