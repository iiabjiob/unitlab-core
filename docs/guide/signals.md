# Signals

## Purpose

Use Signals to import your project signal list, map each signal to a real channel, run quick control checks, and export reports.

## Steps

### Before you start

1. Select the correct workspace.
2. Confirm devices/channels are loaded and online.
3. Prepare an Excel file in one of these formats: `.xls`, `.xlsx`, `.xlsm`.

### Open the import wizard

### Option A: from Home

1. Open Home.
2. Click **Import signal list from project**.
3. The Signals page opens and the modal **Import Signal List** appears.

### Option B: from Signals page

1. Open Signals.
2. Click **+ Import Signal List** in the header.

### Import wizard: full step-by-step

The wizard has 3 steps: **Upload file → Columns → Type mapping**.

### Step 1 — Upload file

1. In **Signal list file**, drag and drop the spreadsheet or click to browse.
2. (Optional) In **Preset**, choose a saved preset, or keep **Manual wizard**.
3. Wait until the file is analyzed and the wizard moves to **Columns**.

If needed:
- Click **Choose different file** to restart with another file.
- Click **Delete selected preset** to remove an old preset.

### Step 2 — Columns

1. In **Worksheet**, choose the sheet to import.
2. In **Columns**, select fields you want to keep.
3. Make sure the **terminal / terminal block / cabinet terminal** column is included (if present in the project file).
4. Use **Select all** or **Clear** for fast selection.
5. Click **Next**.

Required outcome for this step:
- A worksheet is selected.
- At least one column is selected.
- Terminal/cabinet terminal column is selected when available (recommended for cable schedule export).

### Step 3 — Type mapping

1. In **Type column**, choose the vendor column that contains type codes.
2. (Recommended) Confirm/select the **terminal / terminal block** source column used for cabinet wiring reference (if your import flow exposes this mapping in this step).
3. For each detected vendor value, choose an internal type:
	- Digital input (DI)
	- Digital output (DO)
	- Analog input (AI)
	- Analog output (AO)
	- or **Skip**
4. (Optional) Fill **Save as preset**.
5. Click **Import**.

Important behavior:
- Rows mapped to **Skip** are not imported.
- If nothing is mapped, import is blocked.
- Presets should include your terminal-column selection so the same project format can be imported faster next time.

### Allocate channels after import

1. In the grid, find a signal row.
2. In **Unit/Channel**, choose a channel manually, or select multiple rows and click **Allocate**.
3. To remove assignments from selected rows, click **Unassign**.
4. Check the header summary (total / allocated / tested / remaining).

Important behavior:
- Bulk allocation and unassignment run as background jobs.
- Progress is shown in the header and updates in real time.
- When a job completes, the grid is refreshed automatically.

### Run quick control test for selected rows

1. Select allocated physical rows.
2. Click **Run test**.
3. To configure before launch, right-click **Run test** and choose:
	- **Toggle mode**: `Single toggle (ON)` or `Double toggle (ON → OFF)`.
	- **Interval between signals**: `0.5 s`, `1.0 s` (default), `2.0 s`.
4. Watch progress in the header progress bar.

Expected result:
- Success toast with processed/failed summary.
- **Last tested** updates in UnitLab for successful rows with precise per-signal completion time (local test record).

Technical note:
- Test run is executed by backend worker job (not browser loop).
- Progress and completion are delivered over WebSocket job events.
- **Last tested** is a UnitLab-side timestamp and should be compared with the target SCADA/IED/controller view for final validation.

### Create switchgears from selected signals

1. Select DI/DO rows that should form switchgear pairs.
2. Click **Create switchgear**.
3. Review the toast result:
	- full success, or
	- partial creation when there are not enough paired channels.

### Export outputs

- **Export Cable Schedule**: cable allocation export.
- **Export Cable Schedule** is most useful when the signal sheet includes a terminal-block/cabinet-terminal column selected during import.
- **Export Report**: signal report with direction, unit/channel, and last tested timestamp.

## Common issues

### Common errors and how to fix

- **Unsupported file type**: use `.xls`, `.xlsx`, or `.xlsm`.
- **No worksheets / no headers**: verify the first row contains column names.
- **No rows match the selected type mapping**: check type column and mapping values.
- **Allocation incomplete**: run auto-allocation or assign channels manually.

## Next

- [Switchgear](/guide/switchgears)
- [Instructions (Sequences)](/guide/sequences)
- [Troubleshooting](/guide/troubleshooting)
