# Switchgear

## Purpose

Use Switchgear to bind command/feedback channels and send pair commands: **Open**, **Close**, **Undefined**, **Unknown**.

## Steps

### Create or open a switchgear

1. Open **Switchgears**.
2. To create a new item, click **+ New Switchgear** in the sidebar.
3. Select a switchgear from the list to open the editor.

### Configure bindings

1. In the **Bindings** panel, choose mode:
	- **Direct**: bind channels directly.
	- **By Signal**: bind through imported signals (available only when signal sheet exists).
2. Set all required roles:
	- **OPEN position** (DO)
	- **CLOSED position** (DO)
	- **OPEN command from BCU** (DI)
	- **CLOSE command from BCU** (DI)
3. For DI feedback rows, set **Feedback delay** when needed.
4. Use **Reset** to restore default bindings for this switchgear.

### Meaning of Feedback delay

- Delay before DI feedback is evaluated after command execution.
- Use it for slow mechanics and negative-feedback tests.
- `0 ms` means immediate check.

### Send commands safely

1. Confirm both DO command channels are assigned.
2. Confirm both DO channels belong to the same unit.
3. Confirm the unit is online.
4. In toolbar, click one of:
	- **Open**
	- **Close**
	- **Undefined**
	- **Unknown**
5. Watch **Current state** badge and execution log.

### Duplicate or delete switchgear

1. Open the switchgear header menu.
2. Choose **Duplicate** to clone it.
3. Choose **Delete** to remove it and confirm in **Delete switchgear** dialog.

## Verify

Expected result:
- Command is queued.
- ACK/state update is received.
- State badge changes to the target or reported state.

## Common issues

### If a command is blocked

Typical reasons:
- both DO channels are not bound;
- DO channels are on different units;
- target unit is offline;
- WebSocket is disconnected;
- command timed out waiting for feedback.

## Next

- [Signals](/guide/signals)
- [Instructions (Sequences)](/guide/sequences)
- [Troubleshooting](/guide/troubleshooting)
