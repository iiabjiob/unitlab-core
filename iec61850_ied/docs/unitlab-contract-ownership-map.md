# UnitLab contract ownership map

Status: reference map for where each UnitLab contract lives and which layer owns it.

This document exists to prevent contract drift across roadmap, payload schemas, API surface, operating model, and runtime integration notes.

## Ownership map

| Area | Document | Owner | Purpose |
|---|---|---|---|
| Product direction | `docs/iec61850-unitlab-roadmap.md` | UnitLab product layer | High-level product roadmap and PR slicing |
| Cross-layer readiness | `docs/iec61850-unitlab-readiness-matrix.md` | UnitLab product + reusable runtime | What is strong, what blocks, what is optional |
| Slice tracking | `docs/unitlab-product-slice-tracker.md` | UnitLab product layer | PR-by-PR execution checklist |
| Operating model | `docs/unitlab-operating-model.md` | UnitLab product layer | End-to-end verification workflow |
| Verification dataflow | `docs/unitlab-verification-dataflow.md` | UnitLab product layer | Signal list -> planner -> runtime -> evidence -> verdict |
| Contract glossary | `docs/unitlab-contract-glossary.md` | Shared terminology | Canonical field and concept naming |
| Signal planning contract | `docs/unitlab-signal-planning-contract.md` | UnitLab product layer | Target normalization and planning boundary |
| Runtime integration contract | `docs/unitlab-runtime-integration-contract.md` | Product + reusable runtime boundary | Data exchanged between Python and C |
| API payload shapes | `docs/unitlab-api-payload-shapes.md` | UnitLab product layer | Canonical request/response object shapes |
| JSON schema contract | `docs/unitlab-json-schema-contract.md` | UnitLab product layer | Canonical field names and schema rules |
| API surface | `docs/unitlab-api-surface.md` | UnitLab product layer | Proposed route/resource surface |
| Event stream contract | `docs/unitlab-event-stream-contract.md` | UnitLab product layer | Append-only runtime event model |
| State machine contract | `docs/unitlab-state-machine-contract.md` | UnitLab product layer | Canonical runtime and execution states |
| Recovery contract | `docs/unitlab-recovery-contract.md` | Product + runtime boundary | Reconnect, stale, and recovery behavior |
| Verification execution contract | `docs/unitlab-verification-execution-contract.md` | UnitLab product layer | Confirmation/verdict flow |
| Diagnostics contract | `docs/unitlab-diagnostics-contract.md` | Shared runtime + product boundary | Diagnostic shape and severity handling |
| Native runtime audit | `docs/iec61850-native-runtime-audit.md` | Reusable IEC 61850 runtime | Current protocol/runtime strengths and gaps |
| MMS client roadmap | `docs/mms-client-roadmap.md` | Reusable IEC 61850 runtime | Lower-level client/server protocol work |

## Rule of use

- If the topic is product workflow, keep it in the product-layer docs.
- If the topic is reusable IEC 61850 behavior, keep it in the runtime audit or runtime integration docs.
- If a field name appears in more than one doc, the glossary and JSON schema contract define the canonical meaning.
- If a workflow crosses the boundary, the runtime integration contract is the primary handoff document.

## Related docs

- `docs/iec61850-unitlab-roadmap.md`
- `docs/iec61850-unitlab-readiness-matrix.md`
- `docs/unitlab-product-slice-tracker.md`
- `docs/unitlab-operating-model.md`
- `docs/unitlab-verification-dataflow.md`
- `docs/unitlab-contract-glossary.md`

