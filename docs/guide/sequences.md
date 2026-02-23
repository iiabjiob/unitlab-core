# Instructions (Sequences)

## Purpose

Use Instructions (Sequences) to build repeatable multi-step actions and execute them with one **Run** button.

This mode is intended for:
- repeatable functional checks
- timing-sensitive tests
- stress / pulse-train ("storm") tests
- customer demonstration scenarios (same sequence every run)

Use Sequences when you need more than a single manual toggle in Signals or Switchgears.

## Typical sequence use cases (examples)

### 1. Debounce / anti-bounce validation

Goal:
- Verify a target DI input filter ignores short pulses.

Example:
- 10 pulses
- pulse width: 20 ms
- pause between pulses: 20 ms

How to interpret:
- UnitLab sequence **completed** = UnitLab sent the pattern successfully.
- SCADA/IED should remain stable if debounce filtering is configured correctly.
- If SCADA toggles on short pulses, review filter/debounce settings on the target system.
- Confirm that the configured pulse timing (for example 20 ms) is valid for your test stand and target-device processing path.

### 2. Repeated switchgear command pattern

Goal:
- Validate repeated open/close control and indication consistency.

Example:
- `Switch pos -> OPEN`
- `Wait 1 s`
- `Switch pos -> CLOSE`
- repeat 10 times

What to verify:
- no missed transitions
- no unstable intermediate position in the target system
- command/feedback timing stays consistent across cycles

### 3. Group control stress test

Goal:
- Validate cabinet behavior when multiple signals are driven in sequence with short spacing.

Example:
- `Group ctrl` for a selected set of channels
- inter-step wait `50–200 ms`
- repeat several cycles

What to verify:
- target system processes events in expected order
- no unexpected latching/interlocking behavior
- no overloaded operator logic in SCADA/IED

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
2. If the sequence is tied to project signals, verify signal allocations and physical wiring first.
3. Click **Run** in run controls.
4. Watch status badge and progress line (`Progress: X% · step N of M`).
5. Click **Stop** to request cancellation when needed.

Possible statuses include running, completed, cancelling, and error.

Important:
- **Completed** means the sequence executed successfully on the UnitLab side.
- Final acceptance still requires checking the target SCADA/IED/controller reaction.

## Verify

Expected result:
- Step list order and content update immediately.
- Status toasts confirm copy/paste operations.
- Run status/progress reflects the active sequence execution.
- Target system behavior matches the intended sequence pattern.

## Practical example (step sketch): 10 x 20 ms pulse debounce test

One simple pattern you can build:

1. `Pulse` on the target-mapped channel (`20 ms`)
2. `Wait` (`20 ms`)
3. Repeat the pair until you have 10 pulses total

Alternative:
- duplicate the pulse/wait pair steps using copy/paste to build the sequence faster

What to record:
- sequence run result in UnitLab (`completed` / `error`)
- target SCADA/IED indication behavior during the pulse train
- whether debounce filter suppressed all short pulses as expected

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
