import { buildElectricalGraph } from "./graph"
import { parseScdSource } from "./parser"
import { createSldDocumentFromGraph } from "./sldDocument"
import type { GenerateSldOptions, GenerateSldResult, ScdDiagnostic, ScdSource } from "./types"

export function generateSldFromScd(source: ScdSource, options: GenerateSldOptions = {}): GenerateSldResult {
  const model = parseScdSource(source)
  const graph = buildElectricalGraph(model)
  const document = createSldDocumentFromGraph(graph, options)

  return {
    model,
    graph,
    document,
    diagnostics: mergeDiagnostics(model.diagnostics, graph.diagnostics, document.diagnostics),
  }
}

function mergeDiagnostics(...diagnosticGroups: ScdDiagnostic[][]): ScdDiagnostic[] {
  const diagnostics: ScdDiagnostic[] = []
  const seen = new Set<string>()

  for (const diagnostic of diagnosticGroups.flat()) {
    const key = [
      diagnostic.severity,
      diagnostic.stage,
      diagnostic.code,
      diagnostic.sourceId ?? "",
      diagnostic.sourcePath ?? "",
      diagnostic.message,
    ].join("\u0000")
    if (seen.has(key)) {
      continue
    }
    seen.add(key)
    diagnostics.push(diagnostic)
  }

  return diagnostics
}

export { parseScdSource } from "./parser"
export { buildElectricalGraph } from "./graph"
export { createSldDocument, createSldDocumentFromGraph } from "./sldDocument"
export type {
  ElectricalGraph,
  ElectricalGraphEdge,
  ElectricalGraphEdgeKind,
  ElectricalGraphGroup,
  ElectricalGraphGroupKind,
  ElectricalGraphJunction,
  ElectricalGraphNode,
  ElectricalGraphPort,
  GenerateSldOptions,
  GenerateSldResult,
  NormalizedSclModel,
  ScdDiagnostic,
  ScdDiagnosticSeverity,
  ScdDiagnosticStage,
  ScdSource,
  SclBay,
  SclConnectivityNode,
  SclEquipment,
  SclEquipmentKind,
  SclIed,
  SclLogicalNodeRef,
  SclSubstation,
  SclTerminal,
  SclVoltageLevel,
  SldConnection,
  SldDocument,
  SldElement,
  SldElementKind,
  SldLabel,
} from "./types"
