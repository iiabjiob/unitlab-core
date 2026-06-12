# IEC 61850 MMS Client Roadmap

This document tracks the work needed to turn the current native MMS debug client into a production-shaped IEC 61850 client that can discover, subscribe to, and decode live signals from real IEDs.

The target is IEDScout-like base behavior:

- associate with an IEC 61850 MMS server;
- discover the IED logical model;
- discover datasets and report control blocks;
- subscribe to BRCB/URCB reports;
- request GI;
- decode incoming reports into stable data references and values;
- expose enough diagnostics for packet-level comparison against Wireshark/IEDScout captures.

## Current State

Implemented and validated against the local native/libIEC61850-oriented test path:

- COTP/ACSE/MMS association path exists.
- Basic `Read`, `GetNameList`, `GetVariableAccessAttributes`, `GetNamedVariableListAttributes`, and `Write` requests exist.
- GetNameList request building/decoding separates LN selector (`node_id`) from MMS `continueAfter`, while preserving the legacy builder path.
- Discovery runner is isolated from the CLI in `src/server/native_wire_client_discovery.c`.
- Session/discovered/subscription state has started moving into `src/server/native_wire_client_session.*`.
- Session model now stores discovered logical devices, logical nodes, shallow LN data names, GVA component names, and normalized leaf references, not only summary counts.
- Discovery paginates VMD logical devices, domain logical nodes, datasets, BRCBs, and URCB browse calls with stalled-page detection.
- BRCB discovery uses `GetNameList` class 4 and builds `LN$BR$brcbName` references.
- RptEna and GI write paths exist for the selected BRCB.
- Report decoder extracts `RptID`, `DatSet`, `OptFlds`, inclusion bitstring, `dataRef`, values, and reason fields.
- Report decoder compares incoming `dataRef` values against discovered dataset members.
- Real TCP smoke has passed against the local server path: `discover -> rptena -> gi -> disconnect`, with `dataset-match=true`.

## Known Gaps

These are not yet production-client guarantees:

- Discovery still needs a configurable safety cap for extremely large/malformed IED models.
- Pagination is implemented for the current discovery branches but still needs golden-frame coverage from saved large-model captures.
- Logical model discovery stores logical devices, logical nodes, shallow LN data names, and first-level GVA component names and coarse BER type-kind metadata; full typed data object/data attribute trees are not yet modeled.
- BRCB support is ahead of URCB support.
- RCB lifecycle is incomplete for real devices that require reservation, release, purge, or replay handling.
- Report decoding does not yet cover all valid MMS data shapes from real devices.
- Association negotiation and failure handling are not hardened for broad vendor interoperability.
- UI/debug state is not yet backed by a complete per-device client context.
- Golden-frame validation against saved IEDScout/Wireshark captures is not automated.

## Slice 1: Dynamic Discovery Model

Status: complete.

Closed:

- Session-owned DataSet and DataSet member storage is dynamic.
- Session-owned discovered BRCB storage is dynamic and used by `rptena`/`gi` selection.
- Discovery runner uses dynamic local identifier lists for logical devices, logical nodes, datasets, and discovered BRCB names.
- Repeated `discover`/`close-ied` clears dynamic discovery storage.
- DataSet and DataSet member allocation failures are explicit discovery failures.
- Existing real TCP smoke still reports `dataset-match=true`.

Goal: make discovery reliable for real substations where object counts exceed debug limits.

Change boundary:

- Replace fixed-size discovery arrays with dynamically sized session-owned collections.
- Keep ownership inside `UnitLabNativeClientSessionState` or a nested model object.
- Preserve current CLI/debug output format unless a diagnostic field is required.
- Keep discovery runner independent from CLI.

Acceptance criteria:

- Discovery does not truncate at the previous logical node, dataset, BRCB, or dataset member limits.
- Truncation is still detected and reported if allocation fails or a configured safety cap is hit.
- Repeated `discover` clears/rebuilds the model without stale dataset/RCB entries.
- Existing native/libIEC61850 smoke path still reports `dataset-match=true`.

Validation:

- Focused session model tests for append/clear/re-discover behavior.
- Native wire client/session tests.
- Real TCP smoke against local library-backed server.
- Wireshark capture check when the user provides a new large-model dump.

## Slice 2: Complete Discovery Pagination

Status: complete for current discovery branches.

Closed:

- `GetNameList` builder/decoder now keeps LN selector (`node_id`) separate from MMS `continueAfter`.
- Legacy `GetNameList` builder behavior remains compatible for existing tests and callers.
- VMD logical device, domain logical node, dataset, BRCB, and URCB browse paths walk `moreFollows` pages.
- Pagination uses the last returned identifier as `continueAfter`.
- Stalled pagination fails discovery with a protocol diagnostic instead of looping.
- Focused protocol/runtime tests and local TCP smoke cover the implemented path.

Known follow-up:

- LN data-name pagination is currently issued only as a browse/drain path and is not yet stored as a full logical model; that belongs to Slice 3.
- URCB browse pagination is implemented, but URCB control/subscription lifecycle belongs to Slice 4.
- Page-count diagnostics are not yet surfaced in the debug summary.

Goal: match IEDScout-style discovery behavior for devices that return partial lists.

Change boundary:

- Audit every `GetNameList` branch for `moreFollows`.
- Continue using the last returned identifier as `continueAfter`.
- Detect stalled pagination and fail visibly instead of looping forever.
- Keep invoke-id ordering deterministic for golden-frame comparison.

Acceptance criteria:

- VMD logical devices, domain logical nodes, datasets, LN data names, BRCBs, and URCBs can all walk multiple pages.
- Discovery diagnostics report final object count; page-count diagnostics are a follow-up.
- Stalled or malformed pagination produces a clear error state.

Validation:

- Unit tests with synthetic paginated MMS responses.
- Runtime smoke using a fixture/server configured with page-sized responses.
- Packet comparison against saved captures where available.

## Slice 3: Full Logical Model Discovery

Status: in progress.

Closed in current slice:

- Dynamic session-owned storage exists for discovered logical devices.
- Dynamic session-owned storage exists for discovered logical nodes.
- Dynamic session-owned storage exists for shallow LN data names returned by `GetNameList` class 3.
- Dynamic session-owned storage exists for GVA component names and coarse type-kind metadata read from each discovered LN data item.
- Dynamic session-owned storage exists for recursive typed GVA tree nodes with normalized refs and preserved FC/request context when that context is available.
- Dynamic session-owned storage exists for normalized dataset member leaf references with MMS and display forms.
- Discovery populates those collections during `discover`.
- Debug model summary reports `data-names`, `typed-data-names`, `data-components`, `typed-data-components`, `typed-data-nodes`, and `leaf-refs` counts.
- Report decoder stores session-owned mapped entries for reports that include `dataRef`, including normalized display reference, match flags, value summary, and reason summary.
- Reports without `dataRef` are mapped through discovered Dataset order using the inclusion bitstring.
- Latest report entries now retain primitive typed value metadata alongside text summaries: raw BER tag, raw length, bool, signed/unsigned integer, MMS float32, quality raw code/validity for `$q`, string/octet/bit-string/structure classification, and raw report reason code metadata plus semantic reason flags/labels.
- Session tests cover dynamic growth and reset for logical devices, logical nodes, data names, leaf references, and mapped report entries.
- Real TCP GI smoke now also exercises the fixture-backed discovery/report path with deterministic model ordering checks.
- Fixture-backed discover smoke now snapshots deterministic logical-device, dataset, and BRCB ordering.

Remaining:

- External vendor golden-frame validation from saved IEDScout/Wireshark captures is not automated yet.

Goal: build a useful IEC 61850 model, not just enough state to subscribe to one report.

Change boundary:

- Discover and store logical devices, logical nodes, data objects, data attributes, FC, type/structure metadata, datasets, and RCBs.
- Normalize MMS references and IEC 61850 references consistently:
  - MMS form: `LD/LN$FC$DO$DA`
  - display/reference form: `LD/LN.DO.DA`
- Do not mix protocol adapter state with UI state.

Acceptance criteria:

- The discovered model can answer:
  - which datasets exist;
  - which members each dataset contains;
  - which RCB references a dataset;
  - which data attributes are readable/writable;
  - what decoded value type is expected.
- Model output is deterministic for repeated discovers.

Validation:

- Model snapshot tests from fixture responses.
- Real TCP discovery against local library server.
- Manual comparison with IEDScout tree for the same SCD/device when captures are available.

## Slice 4: RCB Lifecycle

Goal: support real BRCB/URCB subscription flows, not only the simple local-server path.

Closed in current slice:

- Native client now tracks the selected discovered BRCB in subscription state.
- `close-ied` and `disconnect` perform explicit cleanup for the selected RCB: disable `RptEna` when enabled and release buffered `ResvTms` ownership when applicable.
- Linked client smoke verifies that cleanup leaves the buffered BRCB disabled and `ResvTms` cleared.
- Linked client smoke also covers URCB enable/GI/disable behavior without buffered reservation cleanup.
- Native client now performs a preflight RCB attribute read before `RptEna` and `GI` control writes.
- `GI` now requires an active subscription state and rejects writes that do not match the selected subscription index.
- Server runtime now treats `Resv` as a real reserve/release field for report controls and release accepts reserved state transitions.

Change boundary:

- Separate BRCB and URCB control logic.
- Read RCB attributes before writes.
- Support reservation behavior:
  - BRCB: `ResvTms` where applicable;
  - URCB: `Resv` where applicable.
- Support `RptEna`, `GI`, `PurgeBuf`, `EntryID`, `TimeOfEntry`, `DatSet`, `ConfRev`, `OptFlds`, `TrgOps`, and `IntgPd` handling.
- Make release/disable explicit on disconnect/close IED where safe.

Acceptance criteria:

- Client can select an RCB by discovered reference.
- Client refuses RptEna if required reservation fails.
- GI only runs after a valid subscription state.
- Disable/release attempts are visible and diagnostic, not silent.

Validation:

- RCB lifecycle unit tests with success/access-denied/type-mismatch responses.
- Real TCP smoke for BRCB.
- URCB smoke once server support or fixture path exists.
- Wireshark comparison against `005_rptEna_libiec61850.pcapng` and `006_GI_libiec61850.pcapng`.

## Slice 5: Report Decoder Hardening

Goal: decode real vendor reports into stable signal updates.

Change boundary:

- Decode all supported `OptFlds` combinations.
- Decode nested MMS values:
  - boolean;
  - integer/unsigned;
  - float32 for MMS floating-point payloads encoded as exponent-width `0x08` plus IEEE754 bytes;
  - bit-string;
  - octet-string;
  - visible-string;
  - binary-time/utc-time;
  - quality;
  - structured values and arrays.
- Preserve raw bytes when typed decoding is incomplete.
- Detect dataset/report mismatch using `DatSet`, `ConfRev`, inclusion bitstring, and `dataRef`.

Acceptance criteria:

- Reports without `dataRef` are mapped through the discovered dataset order. Current implementation stores this mapping in session state for the latest report.
- Reports with `dataRef` are mapped by reference and checked against the dataset model. Current implementation stores this mapping in session state for the latest report.
- Missing values, unknown reasons, unsupported types, and `ConfRev` mismatch are visible diagnostics.
- Decoder never treats an unknown/unsupported value as successful typed data. Current implementation marks unsupported primitive/constructed values explicitly in latest-report entries.

Validation:

- Session mapping tests for latest-report entries, primitive typed value metadata, quality metadata, report reason code/semantic metadata, and Dataset member order lookup.
- Golden report decoder tests from saved frames.
- Real TCP GI report smoke.
- Regression check that current `dataset-match=true` path still passes.

## Slice 6: Association And Transport Interoperability

Goal: make connection setup robust across real IEDs.

Change boundary:

- Harden COTP/session/presentation/ACSE/MMS initiate handling.
- Track max PDU size and negotiated capabilities.
- Handle reject, abort, timeout, close, EOF, and malformed frames explicitly.
- Keep invoke-id lifecycle session-scoped and collision-safe.

Acceptance criteria:

- Failed association has a precise failure phase.
- Client can disconnect and reconnect without stale state.
- Invoke IDs do not collide across concurrent/async flows inside one session.
- Oversized or segmented responses are rejected or handled explicitly.

Validation:

- Association failure tests.
- Replay/parse tests from `001_associate_libiec61850_new.pcapng`.
- Real TCP reconnect smoke.

## Slice 7: Read/Write Data Access

Goal: allow the debug UI and later runtime integration to read live values safely.

Change boundary:

- Implement typed read decode using the discovered model.
- Implement guarded writes only for attributes marked writable or explicitly selected by the operator.
- Do not allow UI selection alone to trigger writes.

Acceptance criteria:

- Read any discovered data attribute by reference.
- Decode structured read responses into the same value representation used by reports.
- Write failures include MMS error/category diagnostics.
- Writes are explicit and traceable.

Validation:

- Read decode tests for supported MMS data types.
- Write response tests for success/access-denied/type-mismatch.
- Real TCP read smoke against local server.

## Slice 8: Golden Capture Validation

Goal: prevent protocol regressions while aligning with IEDScout/Wireshark behavior.

Change boundary:

- Add scripts/tests that consume saved captures from `artifacts/`.
- Compare important frame-level behavior without requiring exact timestamps.
- Keep simulator-only expectations separate from real-device expectations.

Acceptance criteria:

- Captures cover at least:
  - association;
  - SLD/discovery;
  - online/offline;
  - RptEna;
  - GI;
  - information reports.
- Each supported capture has a named expected behavior file.
- Validation output names missing or unsupported protocol features explicitly.

Validation:

- CI/local test target for golden capture parsing where dependencies are available.
- Manual Wireshark check remains accepted until automation covers the frame class.

## Slice 9: Debug UI State Contract

Goal: make the UI useful for operator/developer verification without hiding protocol state.

Change boundary:

- UI actions remain explicit:
  - discover;
  - connect;
  - disconnect;
  - close IED/delete in-memory model;
  - RptEna;
  - GI.
- Backend/client owns session truth.
- UI displays state and diagnostics, not optimistic success.

Acceptance criteria:

- UI shows connected/associated/discovered/subscribed state.
- UI shows selected RCB and dataset.
- UI shows last report values and matched/unmatched dataRefs.
- UI shows errors/timeouts/access-denied states.
- Close IED clears in-memory model and subscription state visibly.

Validation:

- Component/manual visual check for state transitions.
- Runtime smoke with local server.
- Manual Wireshark capture while using UI.

## Slice 10: Multi-IED And Reconnect Readiness

Goal: remove assumptions that only one debug device/session exists.

Change boundary:

- Session context must own all mutable protocol/model/subscription state.
- No global discovered model or subscription state.
- Reconnect starts from a clean or explicitly restored state.

Acceptance criteria:

- Two client sessions can exist without sharing model/report/subscription state.
- Repeated connect/disconnect/discover cycles do not leak stale RCB/dataset state.
- UI can close one IED model without affecting another session.

Validation:

- Multi-session unit tests.
- Reconnect smoke.
- Memory/error-path checks where tooling is available.

## Definition Of Done For Broad Device Interoperability

The client should not be called broadly IEC 61850-compatible until these are true:

- Dynamic discovery and pagination are complete.
- BRCB and URCB lifecycle behavior is explicit and tested.
- Reports can be mapped with and without `dataRef`.
- Association failure/reconnect paths are deterministic.
- Golden captures from `artifacts/` are part of routine validation.
- At least one real-device or IEDScout-equivalent capture for discovery, RptEna, GI, and reports is matched at the behavior level.

## Working Order

Current recommended order:

1. Dynamic discovery model.
2. Complete discovery pagination.
3. Full logical model discovery.
4. RCB lifecycle.
5. Report decoder hardening.
6. Association and transport interoperability.
7. Read/write data access.
8. Golden capture validation.
9. Debug UI state contract.
10. Multi-IED and reconnect readiness.

