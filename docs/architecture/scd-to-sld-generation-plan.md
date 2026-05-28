# SCD to SLD Generation Plan

Status: planned
Last reviewed: 2026-05-28

## Purpose

This document is the implementation plan for generating a single-line diagram (SLD) from an IEC 61850 SCD file.

The module must be designed as a separate core because the same topology and layout logic is expected to move later into a standalone IEC 61850 tool written in C#.

Update this file as each slice starts and finishes:

- `[ ]` not started
- `[~]` in progress
- `[x]` done
- `[!]` blocked or needs a decision

## Product Goal

The operator selects an SCD file. The system parses and normalizes it, builds an electrical topology graph, creates the required diagram cells/elements, lays them out deterministically, and renders an editable SLD.

The generated diagram is a starting point, not an implicit hardware action:

- Importing SCD must not silently bind devices, channels, or runtime commands.
- Creating or updating operational switchgear records must be explicit and reviewable.
- Unsupported or ambiguous SCD topology must remain visible as diagnostics, not hidden by a guessed layout.

## Non-Goals for the First Implementation

- No automatic hardware allocation from SCD.
- No IEC 61850 runtime communication.
- No automatic FAT execution from imported topology.
- No destructive overwrite of manually edited SLD layouts.
- No force-directed layout as the primary layout engine. FAT diagrams need deterministic engineering layout, not visually random graph placement.

## Target Architecture

```text
SCD XML
  |
  v
XML adapter boundary
  |
  v
SCD parser
  |
  v
Normalized SCL model
  |
  v
Electrical topology graph
  |
  v
Cell and equipment model
  |
  v
Deterministic SLD layout
  |
  v
SLD document DTO
  |
  v
UnitLab Vue adapter / persistence / review UI
```

## Ownership Boundaries

### Portable Core

The core owns:

- SCD parsing rules.
- SCL normalization.
- Electrical graph construction.
- Equipment classification.
- Topology diagnostics.
- Cell/bay model generation.
- Deterministic layout.
- Export to a renderer-neutral SLD document DTO.

The core must not import:

- Vue.
- Pinia.
- browser DOM APIs directly.
- UnitLab stores.
- backend REST clients.
- canvas/SVG rendering code.

For the first implementation, keep the core in the frontend repo but framework-neutral, for example:

- `frontend/src/modules/scd-sld-core/`

If the root workspace later supports shared packages, this can move to:

- `packages/scd-sld-core/`

### UnitLab App Integration

The app layer owns:

- File picker and import workflow.
- User-facing diagnostics and review UI.
- Mapping generated DTOs into the current editable SLD view.
- Persistence of import metadata, generated diagram state, and manual overrides.
- Explicit creation/update of UnitLab switchgear entities if the operator confirms it.

### Backend

Backend persistence is needed once generated SLDs must survive workspace reloads across browsers/users.

Backend should eventually own:

- Uploaded SCD file metadata.
- SCD content hash.
- generated SLD document revision.
- manual override overlay.
- import diagnostics history.

## Core Data Contracts

Keep the core contracts JSON-serializable and language-portable. Avoid TypeScript-only behavior that is hard to port to C#.

### `ScdSource`

Input metadata:

- `fileName`
- `contentHash`
- `xmlText`
- optional `workspaceId`

### `NormalizedSclModel`

Normalized SCD content:

- substations
- voltage levels
- bays
- conducting equipment
- terminals
- connectivity nodes
- logical node references
- IED references
- data-source references where available

Stable IDs should be derived from normalized SCD paths, not random IDs.

Example ID shape:

```text
substation:SS1/voltageLevel:VL110/bay:Q01/equipment:QA1
```

### `ElectricalGraph`

Graph model:

- nodes: equipment, busbar sections, external feeders, transformers, grounding points, junctions
- ports: equipment terminals and generated layout anchors
- edges: terminal/connectivity-node relationships
- groups: substation, voltage level, bay/cell
- diagnostics

### `SldCellModel`

Engineering layout model before final coordinates:

- voltage-level lanes
- bay columns
- busbar groups
- equipment stacks
- feeder/transformer exits
- labels
- unresolved/unsupported placeholders

### `SldDocument`

Renderer-neutral output:

- `version`
- `sourceHash`
- `generatedAt`
- `elements`
- `connections`
- `labels`
- `diagnostics`
- `layoutHints`

This DTO must be independent from Vue component state. UnitLab can adapt it into the current SLD editor model.

## Equipment Mapping

Initial supported mappings:

| SCD element | SLD representation | Notes |
| --- | --- | --- |
| `ConductingEquipment type="CBR"` | breaker/switchgear symbol | Must not imply hardware binding. |
| `ConductingEquipment type="DIS"` | disconnector symbol | Visual-only until domain model supports it explicitly. |
| `ConductingEquipment type="BBS"` | busbar / bold line | Group by voltage level and connectivity. |
| `ConductingEquipment type="PTR"` | transformer symbol | Prefer explicit two-winding topology first. |
| `ConductingEquipment type="VTR"` / `TCTR` | measurement symbol or placeholder | Can be rendered quieter than switching equipment. |
| `ConductingEquipment type="IFL"` / line-like equipment | feeder/line exit | Direction inferred from bay topology where possible. |
| `ConductingEquipment type="GND"` | ground symbol | Treat as terminal endpoint. |
| unknown equipment | placeholder with diagnostic | Do not silently drop. |

## Diagnostics Model

Every pipeline stage returns diagnostics:

- `info`
- `warning`
- `error`

Diagnostics should include:

- source path
- equipment name
- normalized ID
- stage
- message
- suggested operator action where useful

Examples:

- missing terminal reference
- terminal references unknown connectivity node
- unsupported conducting equipment type
- bay has no busbar connection
- multiple plausible busbars
- transformer topology has more than two expected winding groups
- duplicate normalized IDs after name cleanup

## Implementation Slices

### Slice 0 - Architecture Plan

Status: `[x]`

Goal:

- Capture the implementation direction and ownership boundaries before coding.

Deliverables:

- `docs/architecture/scd-to-sld-generation-plan.md`

Validation:

- Documentation review.

### Slice 1 - Core Skeleton and Contracts

Status: `[ ]`

Goal:

- Create a framework-neutral core module with stable contracts and no Vue/store dependencies.

Deliverables:

- `frontend/src/modules/scd-sld-core/index.ts`
- `frontend/src/modules/scd-sld-core/types.ts`
- pipeline function stub:
  - `generateSldFromScd(source: ScdSource, options?: GenerateSldOptions): GenerateSldResult`
- contract tests for JSON-serializable result shape

Validation:

- Frontend type-check.
- Unit tests for empty/invalid input diagnostics.

### Slice 2 - XML Adapter Boundary

Status: `[ ]`

Goal:

- Parse XML without coupling the core to browser DOM APIs.

Recommended approach:

- Add a tiny XML node adapter interface consumed by the core.
- Web integration may use browser `DOMParser`.
- Tests may use a deterministic fixture adapter.
- Future C# port can implement the same traversal contract.

Deliverables:

- `XmlDocumentAdapter`
- `XmlElementAdapter`
- parse error diagnostics
- namespace-tolerant element lookup helpers

Validation:

- Fixtures with namespace-prefixed and non-prefixed SCL tags.
- Invalid XML produces diagnostics, not uncaught exceptions.

### Slice 3 - Minimal SCD Parser

Status: `[ ]`

Goal:

- Extract the subset needed for first SLD generation.

Parse:

- `SCL`
- `Substation`
- `VoltageLevel`
- `Bay`
- `ConductingEquipment`
- `Terminal`
- `ConnectivityNode`
- `LNode`
- `IED` references where reachable

Deliverables:

- raw parsed model
- source-path tracking
- parser diagnostics
- fixture SCD files covering one voltage level and multiple bays

Validation:

- Golden parser snapshots.
- Missing optional sections are handled explicitly.

### Slice 4 - Normalized SCL Model

Status: `[ ]`

Goal:

- Convert raw parsed SCD data into a stable, deduplicated, deterministic model.

Normalization rules:

- stable IDs from SCD hierarchy/path
- trimmed names and descriptions
- explicit parent links
- terminal references resolved to connectivity nodes
- equipment type normalized into known categories
- unresolved references kept with diagnostics

Deliverables:

- `normalizeSclModel(raw): NormalizedSclModel`
- normalization diagnostics
- duplicate-name strategy

Validation:

- Golden normalized model snapshots.
- Duplicate and missing-reference fixtures.

### Slice 5 - Electrical Graph Builder

Status: `[ ]`

Goal:

- Build a topology graph from normalized SCL.

Graph rules:

- equipment becomes graph nodes
- terminals become ports
- connectivity nodes become graph junctions
- busbar sections are detected and grouped
- bay/cell membership remains explicit
- dangling topology remains visible

Deliverables:

- `buildElectricalGraph(model): ElectricalGraph`
- graph diagnostics
- helpers to query bay, voltage-level, busbar, and feeder relationships

Validation:

- Graph golden snapshots.
- Graph invariants:
  - every edge references existing ports/nodes
  - every diagnostic references a source path where possible
  - unknown equipment is preserved

### Slice 6 - Cell Model and Layout MVP

Status: `[ ]`

Goal:

- Produce deterministic engineering layout for common FAT diagrams.

Layout policy:

- voltage levels render as horizontal sections
- busbars render as horizontal bold lines
- bays render as ordered columns/cells
- switching equipment stacks vertically inside each bay
- transformer/feeders exit from the bay edge
- edges are orthogonal where possible
- all coordinates snap to the SLD grid

Deliverables:

- `buildSldCellModel(graph): SldCellModel`
- `layoutSld(cellModel): SldDocument`
- layout options for grid size, spacing, orientation, and label density

Validation:

- Golden layout snapshots.
- Coordinate invariant tests:
  - no `NaN`
  - grid-aligned anchors
  - stable output order
  - deterministic output for same input

### Slice 7 - UnitLab SLD Adapter

Status: `[ ]`

Goal:

- Adapt generated `SldDocument` into the current SLD editor without making the editor own SCD logic.

Recommended behavior:

- Generated visual equipment is imported as SLD diagram elements first.
- Creating/updating operational UnitLab switchgear records is a separate explicit confirmation step.
- Generated elements keep `sourceId` and `sourceHash` for future reconciliation.

Deliverables:

- adapter from `SldDocument` to current SLD editor element model
- import preview state
- apply action that updates diagram state through the same persistence path as manual editing

Validation:

- Existing manual SLD interactions still work after import.
- Generated lines do not break drag, selection, context menu, or grid snap.

### Slice 8 - Import Review UX

Status: `[ ]`

Goal:

- Let the operator understand what will be created before applying the import.

UX:

- file picker
- import summary
- generated preview
- diagnostics panel
- unsupported/ambiguous equipment list
- apply/cancel
- optional explicit "create/update switchgear records" step

Deliverables:

- SCD import panel or modal
- diagnostics list grouped by severity and SCD path
- generated diagram preview

Validation:

- Import with warnings can be reviewed and cancelled.
- Import with blocking errors cannot be applied.
- No hardware-facing action is triggered by preview.

### Slice 9 - Persistence and Regeneration

Status: `[ ]`

Goal:

- Preserve generated SLD state and manual edits without destructive regeneration.

Recommended model:

- base generated document:
  - source SCD hash
  - generated SLD DTO
- manual overlay:
  - moved elements
  - renamed labels
  - hidden placeholders
  - manually added lines/text/symbols
- regeneration:
  - match by stable `sourceId`
  - keep manual overrides where possible
  - surface conflicts instead of overwriting

Deliverables:

- local persistence first, if backend persistence is not ready
- backend persistence proposal before schema changes
- regeneration diagnostics

Validation:

- Reimport same SCD is stable.
- Reimport changed SCD preserves manual moved elements with matching source IDs.
- Removed SCD equipment is flagged before deletion.

### Slice 10 - Fixture Corpus and Real-World Compatibility

Status: `[ ]`

Goal:

- Build confidence against real vendor SCD variations.

Fixtures:

- minimal one-bay SCD
- two-bay busbar SCD
- bus coupler SCD
- transformer bay SCD
- feeder-only SCD
- disconnected/invalid references
- vendor-specific naming conventions

Validation:

- golden parser snapshots
- golden graph snapshots
- golden layout snapshots
- diagnostics snapshots

### Slice 11 - Performance and Safety Gates

Status: `[ ]`

Goal:

- Keep import responsive and deterministic for realistic substations.

Targets to define after fixture corpus:

- max parse time for medium SCD
- max layout time
- max generated element count before UI warning
- memory budget

Validation:

- benchmark fixtures
- no full app freeze during import preview
- cancellation path for large files
- diagnostics for oversized/unsupported topology

### Slice 12 - C# Migration Readiness

Status: `[ ]`

Goal:

- Keep the core portable enough to move to a future IEC 61850 C# tool.

Rules:

- no Vue/browser/backend imports in core
- no reliance on JS object key order for semantic output
- deterministic sorting everywhere
- JSON schema or equivalent contract for public DTOs
- golden test fixtures shared across TypeScript and future C#

Deliverables:

- DTO schema documentation
- fixture bundle
- migration notes
- list of TypeScript helpers that need C# equivalents

Validation:

- Same fixture input produces stable JSON output.
- No UI-only concepts leak into core contracts.

## Recommended First MVP

The first useful MVP should support:

- one SCD file selected from the switchgear SLD page
- parse/normalize diagnostics
- voltage levels
- bays/cells
- busbar as bold line
- breakers as switchgear-like visual symbols
- disconnectors and ground as visual static symbols/placeholders
- text labels
- orthogonal lines
- preview before apply
- no runtime binding and no automatic hardware action

This gets value quickly while keeping safety boundaries intact.

## Open Decisions

- Whether generated CBR equipment should create UnitLab switchgear domain records automatically after operator confirmation, or remain visual-only until explicitly promoted.
- Backend schema for SCD imports and generated SLD revisions.
- Exact supported subset of IEC 61850 SCL for the first production import.
- Whether the current SLD editor model needs a separate generated-equipment element type before import.
- How to represent disconnectors visually if the current switchgear symbol remains breaker-only.
- How much vendor-specific naming cleanup belongs in the core vs app-level import profiles.

## Runtime and Safety Risks

- SCD topology can be incomplete or vendor-specific; the system must show diagnostics instead of silently generating a misleading diagram.
- A generated visual breaker is not proof of a controllable UnitLab switchgear or hardware channel.
- Regeneration can destroy operator edits if stable IDs and manual overlays are not implemented early.
- Layout quality depends on deterministic bay/busbar inference; ambiguous busbar topology must be visible to the operator.
- Future C# migration will be expensive if Vue/browser APIs leak into the core.

## Validation Checklist

- Parser handles valid and invalid SCD without uncaught exceptions.
- Normalized IDs are stable across repeated imports.
- Graph output preserves unsupported equipment as placeholders.
- Generated layout is deterministic and grid-aligned.
- Import preview cannot trigger hardware or test execution.
- Applying import does not erase manual SLD edits without explicit operator confirmation.
- Reimport with same SCD hash is idempotent.
- Diagnostics are visible and actionable.
