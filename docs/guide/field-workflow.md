# Field Workflow (Engineer User Guide)

## What this system is for

This system replaces manual jumpers with controllable peripheral units (DI/DO/AO) so an engineer can verify cabinet wiring and signal behavior faster and more consistently.

Core idea:
- Your peripheral units act as digital "jumpers".
- You drive signals from UnitLab and observe the result in the target SCADA/IED/controller.
- You compare the expected signal names (from the project signal list) with the actual behavior on the target system.

## What the engineer needs to know first

### Physical setup model

- UnitLab server runs on an `RPi5` and hosts a Wi-Fi access point.
- Peripheral units (`DI`, `DO`, `AO`) connect automatically to this access point.
- Each unit has a unique identifier (example: `DO-001`, `DI-001`, `AO-001`).
- The engineer connects a laptop/tablet to the same Wi-Fi AP and opens the web UI in a browser.

### Wi-Fi access point naming and password

Central module access point uses a predictable pattern:
- **SSID**: `[unitlab]-core-ABCD`
- **Password**: `pwd!ABCD`

Where:
- `ABCD` is the tail/suffix of the access point identifier (shown on the central module / AP label).
- The same suffix is used in both SSID and password.

Example:
- SSID: `[unitlab]-core-7F2A`
- Password: `pwd!7F2A`

### Web interface address (default)

After connecting to the UnitLab Wi-Fi AP, open the browser at:
- **`http://10.42.0.1`** (default UnitLab core / RPi5 AP address)

Notes:
- Your laptop will usually receive an address in the same subnet (`10.42.0.x`).
- If the page does not open, check that the laptop is connected to the UnitLab AP (not to another Wi-Fi network).

No additional client setup is required:
- no local software install
- no device-specific passwords in UI
- only the Wi-Fi password

### Two ways to work in the system

1. **Direct hardware mode**
- Work directly with units/channels (no signal list required).
- Useful for quick bench checks or emergency field actions.

2. **Signal-list mode (recommended for cabinet validation)**
- Import a project signal list from Excel.
- Allocate UnitLab channels to project signals.
- Run tests and compare UnitLab results with SCADA signal names.

The signal list is optional, but it makes verification much faster and easier to read.

## Typical engineer workflow (end-to-end)

## Step 1 — Connect to the UnitLab network and open the web UI

1. Power the UnitLab server and peripheral units.
2. Connect your laptop to the UnitLab Wi-Fi access point (`[unitlab]-core-ABCD`).
3. Enter the Wi-Fi password (`pwd!ABCD`, where `ABCD` matches the AP suffix).
4. Open the UnitLab web interface in the browser at `http://10.42.0.1`.
5. Wait until the main UI is loaded.

Expected result:
- Web UI opens.
- System status is healthy.

## Step 2 — Confirm units are online and identify them by `unit_id`

1. Open **Devices**.
2. Confirm required units are visible and online (for example `DO-001`, `DO-002`, `DI-001`).
3. Use the unique `unit_id` to identify the physical module.

Why this matters:
- `unit_id` is the stable link between the physical box and the UI.
- During field work, you will use `unit_id/channel` (example `DO-001/ch3`) for connection and troubleshooting.

Tip:
- If a unit is missing, check power, Wi-Fi coverage, and physical module status LEDs first.

## Step 3 — Choose the working mode

### Option A — Direct hardware mode (no signal list)

Use this when:
- you need to drive a channel immediately
- the project signal list is not available
- you are doing quick diagnostics

Workflow:
1. Open **Devices**.
2. Operate channels directly.
3. Observe response in target controller/SCADA.

### Option B — Signal-list mode (recommended)

Use this when:
- you validate a real cabinet/project
- you want readable names matching SCADA
- you need repeatable testing and reporting

Continue with Steps 4–8.

## Step 4 — Import the project signal list (Excel)

1. Open **Signals**.
2. Click **+ Import Signal List**.
3. In the import wizard:
   - Upload Excel file (`.xls`, `.xlsx`, `.xlsm`)
   - Select worksheet
   - Select columns to keep
   - Select the **terminal / terminal block / cabinet terminal** column (recommended for wiring)
   - Map project signal types to internal types (`DI/DO/AI/AO`)
4. Complete import.

Important:
- The imported signal list is used as an operational working copy for this workspace.
- It is not treated as the permanent source of truth for the project.
- You can re-import when the project signal list changes.
- If available in the project file, select the terminal-block column during import so the **Cable Schedule** shows the exact cabinet terminal point for the wiring technician.

## Step 5 — Allocate UnitLab channels to project signals

### What allocation means

Allocation links a signal list row to a real UnitLab channel:
- Example: `Project DI signal` -> `DO-001/ch3`

This lets UnitLab drive the cabinet input physically and track test status for that project signal.

### How to allocate (recommended order)

1. In **Signals**, filter and sort the rows you want to test now.
2. Select rows (checkboxes).
3. Use:
   - **Assign Hardware** (bulk allocation via background job), or
   - manual allocation for individual rows
4. Confirm the `Unit/Channel` column is filled.

Allocation behavior:
- Online devices are preferred first.
- Free channels are chosen in channel-index order inside a device when auto-allocation is used.

Field note:
- Allocation is intentionally **live**, not frozen.
- Engineers can re-allocate signals during work (`test -> rewire -> re-allocate -> test again`).

## Step 6 — Physically connect UnitLab channels to the cabinet terminals

1. Export **Cable Schedule** from **Signals**.
2. Use the exported table to wire the cabinet:
   - signal list row (project name)
   - `unit_id`
   - `channel_index`
3. Connect UnitLab channels to the target terminal block / cabinet terminals.

Why this helps:
- The cable schedule is the handoff document for physical connection.
- It removes guesswork during wiring and rewiring.

## Step 7 — Run a test from Signals

1. In **Signals**, select the allocated rows you want to test.
2. Click **Run test**.
3. (Optional) Configure:
   - toggle mode (`single` / `double`)
   - interval between signals
4. Watch test progress in the UI.

What happens during the run:
- UnitLab sends commands to allocated channels (usually DO channels for DI checks in the target cabinet).
- Successful test actions update **Last tested** for each signal row **in UnitLab** (local test record).
- Progress and results are visible in the header/global status.

## Step 8 — Compare UnitLab results with SCADA / controller signal list

This is the key validation step.

Use two lists side-by-side:
1. **UnitLab Signals**
   - project signal names
   - `Unit/Channel`
   - `Last tested`
2. **Target SCADA / IED / controller view**
   - signal names/states seen by the target system

How to compare:
- Run a signal (or batch).
- Observe the corresponding indication in SCADA.
- Confirm the expected signal name and expected behavior match.
- Sort UnitLab by **Last tested** to inspect the exact activation sequence.

If something does not match:
- check physical wiring using the cable schedule
- verify channel allocation (`unit_id/channel`)
- re-run the affected signal(s)

## Fast field operation: one signal on demand

Common field scenario:
- Customer asks: "Can you switch signal X now?"

Use **Signals**:
1. Search the signal name in the grid.
2. Use the **Control** action on that row.
3. Toggle ON/OFF.
4. Observe result in SCADA/IED.

This is much faster than manual jumpers when the signal is already allocated.

## Switchgear workflow (pair control / feedback)

Use **Switchgears** when you need schematic-level open/close behavior instead of single-channel control.

Typical pattern:
- Two channels represent Double Point Command / Double Point State
- UnitLab drives the configured pair and shows interpreted position:
  - `OPEN`
  - `CLOSED`
  - `UNKNOWN`
  - `UNDEFINED`

Use this for:
- breakers / disconnectors / earth switches
- repeated operator-like control checks

## Sequences workflow (special and stress tests)

Use **Instructions (Sequences)** when a single signal toggle is not enough and you need a repeatable timing pattern.

Typical use cases:
- debounce / anti-bounce validation
- pulse-train ("storm") checks
- repeated open/close timing patterns
- grouped multi-channel actions
- repeatable FAT/SAT demonstrations for a customer

Why use Sequences:
- exact repeatability (same steps and timing)
- one-button execution
- easier comparison with SCADA/IED behavior during demonstrations and retests

### Example: debounce filter check (anti-bounce)

Goal:
- verify the target input filtering logic ignores short pulses

Example test pattern:
- 10 pulses
- pulse width: 20 ms
- pause between pulses: 20 ms (or your project-defined value)

Expected result:
- UnitLab sequence completes successfully (commands were sent as configured)
- Target SCADA/IED/controller input remains stable if debounce filtering is correct
- If the target system toggles on these pulses, debounce/filter settings may be too low

Important:
- Sequence completion confirms execution on the **UnitLab** side
- Final acceptance is based on the target system response

## How to read and trust the result

A signal is considered tested when:
- UnitLab successfully issued the control action to the allocated channel, and
- the action was confirmed by the runtime path used by the system

Important:
- **Last tested** is a UnitLab-side test record timestamp.
- It is **not** direct confirmation from the target SCADA/IED/controller.
- Final verification is done by comparing UnitLab results with the target system indications (Step 8).

Practical indicators:
- **Last tested** timestamp on the signal row
- test-run summary (`ok / skipped / remaining`)
- execution logs/toasts

## Recommended daily workflow (short version)

1. Connect to UnitLab Wi-Fi and open web UI.
2. Check **Devices** and confirm required `unit_id`s are online.
3. Open **Signals** and import/update project signal list (if needed).
4. Filter/select working batch and allocate channels.
5. Export cable schedule and connect the cabinet.
6. Run test for selected signals.
7. Compare UnitLab sequence (`Last tested`) with SCADA indications.
8. Re-allocate and re-run as needed.
9. Export report for evidence.
10. Use **Instructions (Sequences)** for timing-sensitive and stress tests (debounce / pulse storms / repeatable patterns).

## Where to go next

- [Getting Started](/guide/quick-start)
- [Signals](/guide/signals)
- [Switchgear](/guide/switchgears)
- [Instructions (Sequences)](/guide/sequences)
- [Troubleshooting](/guide/troubleshooting)
