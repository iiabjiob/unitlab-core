# FAQ

## Where should I start if this is my first shift?

Start with [Getting Started](/guide/quick-start), then open [Interface](/guide/interface) to learn where each operation lives.

## Why is workspace selection so important?

Signals, Switchgears, and Instructions are workspace-scoped. If the wrong workspace is active, you may see missing data or control the wrong assets.

## What should I check before sending any command?

Minimum checklist:
- active workspace;
- target unit online;
- required bindings configured;
- current state verified in UI.

Detailed flow: [Getting Started](/guide/quick-start).

## The command was sent, but state did not change. What now?

Check DI feedback bindings and tune **Feedback delay**. Also verify physical device movement/feedback path.

Playbook: [Troubleshooting](/guide/troubleshooting).

## Import wizard fails or imports incorrect signal rows. How do I fix it?

Re-check:
- worksheet and selected columns;
- type column;
- DI/DO/AI/AO mapping;
- rows accidentally mapped to **Skip**.

Full procedure: [Signals](/guide/signals).

## Auto-allocation left some signals unassigned. Is that normal?

Yes, if there are not enough valid free channels. Manually assign remaining rows or adjust selection and retry.

Steps: [Signals](/guide/signals).

## Why can’t I run an instruction?

Common causes:
- instruction is **Read-only**;
- current state does not allow start;
- invalid step/channel configuration.

Run workflow: [Instructions (Sequences)](/guide/sequences).

## Can I edit system-managed read-only instructions?

Not directly. Duplicate the instruction, then edit and run the copy.

## Where do I see what exactly happened after an action?

Use all three:
- toast notification;
- state badge/grid value;
- execution/event log.

## What should I include when reporting an incident?

Send:
- workspace name;
- module + exact action;
- exact error text;
- timestamp;
- screenshot with status and log visible.

Escalation checklist: [Troubleshooting](/guide/troubleshooting).
