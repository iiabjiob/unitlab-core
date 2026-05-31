import { buildSldCellModel } from "./cellModel"
import { buildElectricalGraph } from "./graph"
import { layoutSldDocument } from "./layout"
import { parseScdSource } from "./parser"
import type { GenerateSldOptions, GenerateSldResult, ScdDiagnostic, ScdSource } from "./types"

export function generateSldFromScd(source: ScdSource, options: GenerateSldOptions = {}): GenerateSldResult {
  const model = parseScdSource(source)
  const graph = buildElectricalGraph(model)
  const cellModel = buildSldCellModel(graph)
  const document = layoutSldDocument(cellModel, graph, options)

  return {
    model,
    graph,
    cellModel,
    document,
    diagnostics: mergeDiagnostics(model.diagnostics, graph.diagnostics, cellModel.diagnostics, document.diagnostics),
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
      diagnostic.sourceLocation?.line ?? "",
      diagnostic.sourceLocation?.column ?? "",
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

export { buildIec61850ReportSubscriptionInventory, parseScdSource } from "./parser"
export { parseSclDataTypeTemplates } from "./datatypeTemplates"
export { buildElectricalGraph } from "./graph"
export { buildSldCellModel } from "./cellModel"
export { layoutSldDocument } from "./layout"
export { FEEDER_TEMPLATE_UNITS, getSldBayLayoutTemplates } from "./bayTemplates"
export { createFlatSldDocument, createFlatSldDocumentFromGraph } from "./sldDocument"
export type {
  SldBayLayoutTemplate,
  SldBayLayoutTemplateLabel,
  SldBayLayoutTemplatePoint,
  SldBayLayoutTemplateSlot,
  SldBayLayoutTemplateWire,
} from "./bayTemplates"
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
  Iec61850ReportSubscriptionCandidate,
  NormalizedDataLeaf,
  NormalizedDatasetEntry,
  NormalizedSclModel,
  ScdDiagnostic,
  ScdDiagnosticSeverity,
  ScdDiagnosticStage,
  ScdSource,
  ScdSourceLocation,
  SclAccessPoint,
  SclAddressParameter,
  SclBay,
  SclConnectedAccessPoint,
  SclConnectivityNode,
  SclDataSet,
  SclBda,
  SclDa,
  SclDaType,
  SclDataSetMember,
  SclDataSetMemberKind,
  SclDataTypeTemplatesModel,
  SclDo,
  SclDoType,
  SclEnumType,
  SclEnumVal,
  SclEquipment,
  SclEquipmentKind,
  SclIed,
  SclLogicalDevice,
  SclLogicalNode,
  SclLogicalNodeRef,
  SclReportClient,
  SclReportControl,
  SclReportEnabled,
  SclReportOptionalFields,
  SclReportTriggerOptions,
  SclServer,
  SclSubstation,
  SclTerminal,
  SclVoltage,
  SclVoltageLevel,
  SldBayCell,
  SldBayEarthSwitchPlacement,
  SldBayEquipmentRole,
  SldBayInterpretation,
  SldBayLayoutOrientation,
  SldBayLayoutVariant,
  SldBayOutgoingSide,
  SldCellModel,
  SldCellNode,
  SldCellNodeRole,
  SldConnection,
  SldConnectionRoute,
  SldConnectionRouteSegment,
  SldCoordinate,
  SldDocument,
  SldElement,
  SldElementDimensions,
  SldElementKind,
  SldElementOrientation,
  SldElementRepresentation,
  SldElementStrokeWeight,
  SldElementVisual,
  SldLabel,
  SldRoutePoint,
  SldVoltageLevelLane,
} from "./types"
