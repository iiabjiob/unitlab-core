# Instructions (Sequences)

## Purpose

Use Instructions to build repeatable multi-step actions and execute them with one **Run** button.

## Steps

### Open or create an instruction

1. Open **Instructions**.
2. Select an instruction in the sidebar.
3. To create one, click **+ New Instruction**.
4. To import from JSON, click **Import Instructions**.

### Manage instruction metadata

In the instruction header menu:
- **Rename**
- **Edit description**
- **Duplicate**
- **Export**
- **Delete**

Read-only instructions show a **Read-only** badge and cannot be modified directly.

### Add steps

Use the add toolbar buttons:
- **Wait**
- **Latch**
- **Pulse**
- **Switch pos**
- **Group ctrl**
- **AO**

### Edit a step

1. Click a step in the list.
2. The step editor opens on the right.
3. Update fields for that step type.
4. Changes are saved through the editor workflow.

### Reorder / copy / paste / delete steps

1. Drag steps to reorder.
2. Select one or multiple steps (Shift/Ctrl selection supported).
3. Use copy/paste actions to clone and insert step groups.
4. Delete selected steps when no longer needed.

### Run and stop instruction

1. Verify channels/devices are ready.
2. Click **Run** in run controls.
3. Watch status badge and progress line (`Progress: X% · step N of M`).
4. Click **Stop** to request cancellation when needed.

Possible statuses include running, completed, cancelling, and error.

## Verify

Expected result:
- Step list order and content update immediately.
- Status toasts confirm copy/paste operations.

## Common issues

### If run is unavailable

- The instruction may be read-only.
- Current status may not allow start/stop transition.
- Required channels/devices may be offline or misconfigured.

Use [Troubleshooting](/guide/troubleshooting) for diagnostics.

## Next

- [Signals](/guide/signals)
- [Switchgear](/guide/switchgears)
- [Troubleshooting](/guide/troubleshooting)
