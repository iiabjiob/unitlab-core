# Troubleshooting

## Purpose

Use this page to diagnose incidents quickly using the same flow in every case: **Symptoms → Checks → Fix → Verify**.

## Steps

1. Match your issue to the closest case below.
2. Run checks in order.
3. Apply the listed fix.
4. Confirm recovery in the verify block.

## 1) Device or unit is offline

Symptoms:
- Commands are disabled, rejected, or fail immediately.

Checks:
1. Confirm correct workspace is selected.
2. Open related module (Signals/Switchgears/Instructions) and verify online badges.
3. Confirm network/device availability outside UI if needed.

Fix:
- Restore connectivity/power for the target unit.
- Refresh the page/module after device returns online.

Verify:
- Online badge returns.
- Command buttons become available.

## 2) Switchgear command is rejected

Symptoms:
- Warning/error toast on **Open/Close/Undefined/Unknown**.

Checks:
1. In Switchgear **Bindings**, verify both DO command roles are assigned.
2. Confirm both DO channels are on the same unit.
3. Confirm WebSocket connection is active.

Fix:
- Correct bindings and unit mismatch.
- Reconnect communication channel if WebSocket is down.

Verify:
- Command is queued successfully.
- State/log updates after ACK.

## 3) Command queued but no state confirmation

Symptoms:
- Pending/timeout behavior, missing feedback transition.

Checks:
1. Verify DI feedback bindings (**IED OPEN command**, **IED CLOSE command**).
2. Check **Feedback delay** value for slow mechanics.
3. Confirm physical equipment can actually move/change state.

Fix:
- Correct DI feedback channels.
- Increase **Feedback delay** incrementally.

Verify:
- State badge changes reliably after command.

## 4) Signal import fails or produces bad rows

Symptoms:
- Import wizard errors, empty imported set, wrong types/columns.

Checks:
1. File type must be `.xls`, `.xlsx`, or `.xlsm`.
2. In wizard **Columns** step, verify worksheet and selected columns.
3. In **Type mapping**, verify vendor type column and DI/DO/AI/AO mapping.
4. Confirm rows are not all set to **Skip**.

Fix:
- Re-run import with corrected worksheet/column/type mapping.
- Save a valid preset after successful import.

Verify:
- Grid contains expected signals.
- Header totals/allocated counts are plausible.

## 5) Allocation or test run in Signals behaves unexpectedly

Symptoms:
- Auto-allocation leaves rows unassigned, run test skips many rows.

Checks:
1. Confirm selected rows are of valid direction/type for the action.
2. Confirm enough free channels exist.
3. Confirm target units are online for controllable rows.

Fix:
- Allocate remaining rows manually.
- Reduce selection to rows with valid physical targets.

Verify:
- Success toast reports assigned/processed counts.
- **Last tested** updates for successful rows.

## 6) Instruction run fails or cannot start

Symptoms:
- **Run** unavailable, immediate error, or run stops unexpectedly.

Checks:
1. Confirm instruction is not **Read-only**.
2. Review current status and `last_error` in run controls.
3. Open failing step and validate channel/timing parameters.

Fix:
- Duplicate read-only instruction, edit the copy.
- Correct invalid step parameters or unavailable channels.

Verify:
- Run starts and progresses (`step N of M`) without immediate error.

## Escalation checklist (before contacting support)

Provide:
- active workspace;
- module and exact action;
- exact toast/error text;
- timestamp;
- screenshot including status/log.

## Next

- [Signals](/guide/signals)
- [Switchgear](/guide/switchgears)
- [Instructions (Sequences)](/guide/sequences)
- [FAQ](/guide/faq)
