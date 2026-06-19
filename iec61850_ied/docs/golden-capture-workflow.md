# IEC 61850 Golden Capture Workflow

This package keeps ad hoc packet dumps separate from validation-grade golden captures. The golden capture runner only reads files listed in `docs/golden-captures.json`.

## Directory Layout

- `artifacts/golden/association/` contains association and connection-state captures that are stable enough for automated gates.
- `artifacts/golden/scd/` contains SCD-backed discovery, RptEna, GI, and report-flow captures used for UnitLab-vs-IEDScout behavior checks.
- `artifacts/golden/discovery/` contains discovery-only captures, including full-model browse traffic without report subscription.
- `artifacts/raw/legacy/` contains old exploratory captures, failed attempts, and dumps that are not currently accepted as golden gates.

`artifacts/golden/**/*.pcapng` is tracked in git so a fresh checkout can run the golden gates without private local files. Raw captures under `artifacts/raw/` remain ignored. The manifest plus SHA256 values in `docs/golden-captures.json` is the validation index for those tracked pcaps.

## Naming

Golden capture names should follow:

```text
<source>-<scope>-<behavior>.pcapng
```

Examples:

- `libiec61850-association-happy.pcapng`
- `unitlab-scd-discover-rptena-gi-reports.pcapng`
- `iedscout-scd-discover-rptena-gi-reports.pcapng`

Avoid sequence numbers like `-03` or `first-test` in golden names. Keep those in `artifacts/raw/legacy/` until they are promoted.

## Promotion Rules

1. Capture into `artifacts/raw/` first.
2. Validate manually with Wireshark/tshark and the focused checker script.
3. Rename into the matching `artifacts/golden/<scope>/` folder.
4. Add or update the manifest entry with the expected SHA256 and checker args.
5. Run `iec61850_ied/scripts/run-golden-capture-gates.py --require-tools`.

A capture is not a golden gate until it has a manifest entry and passes the runner.

## Deferred Captures

Deferred captures stay named in the manifest when they are useful evidence but not yet valid gates. Current deferred reasons include missing negative association artifacts, captures that start after association, and native-wire captures whose RCB namespace does not match the current checker expectations.
