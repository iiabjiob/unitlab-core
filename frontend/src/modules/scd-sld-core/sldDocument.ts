import type { ElectricalGraph, GenerateSldOptions, NormalizedSclModel, SldDocument } from "./types"
import { buildElectricalGraph } from "./graph"
import { createBaseSldDocumentFromGraph } from "./sldDocumentBase"

/**
 * Debug/fallback document builder.
 *
 * Production SCD imports must use generateSldFromScd(), which runs:
 * parse -> electrical graph -> SLD cell model -> deterministic layout.
 * This helper intentionally maps graph nodes one-to-one into unlaid-out
 * SLD elements so graph output can be inspected without implying that
 * ElectricalGraphNode is the final visual element contract.
 */
export function createFlatSldDocument(model: NormalizedSclModel, options: GenerateSldOptions = {}): SldDocument {
  return createFlatSldDocumentFromGraph(buildElectricalGraph(model), options)
}

export function createFlatSldDocumentFromGraph(graph: ElectricalGraph, options: GenerateSldOptions = {}): SldDocument {
  return {
    ...createBaseSldDocumentFromGraph(graph, options),
    layoutHints: {
      generatedFrom: "scd-flat-debug",
      gridSize: options.gridSize ?? 24,
    },
  }
}
