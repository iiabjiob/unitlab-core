import { parseScdSource } from "./parser"
import { createSldDocument } from "./sldDocument"
import type { GenerateSldOptions, GenerateSldResult, ScdSource } from "./types"

export function generateSldFromScd(source: ScdSource, options: GenerateSldOptions = {}): GenerateSldResult {
  const model = parseScdSource(source)
  const document = createSldDocument(model, options)

  return {
    model,
    document,
    diagnostics: [...model.diagnostics, ...document.diagnostics.filter(item => !model.diagnostics.includes(item))],
  }
}

export { parseScdSource } from "./parser"
export { createSldDocument } from "./sldDocument"
export type {
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
