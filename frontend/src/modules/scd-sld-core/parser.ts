import type { NormalizedSclModel, ScdDiagnostic, ScdSource } from "./types"
import {
  buildIec61850ReportSubscriptionInventory,
  normalizeReportDataSetReferences,
  parseIedCommunicationModel,
} from "./communicationParser"
import { parseSclDataTypeTemplates } from "./datatypeTemplates"
import { normalizeTerminalConnectivityReferences } from "./topologyNormalizer"
import { parseSclTopology } from "./topologyParser"

export function parseScdSource(source: ScdSource): NormalizedSclModel {
  const diagnostics: ScdDiagnostic[] = []
  const model: NormalizedSclModel = {
    schema: "unitlab.scd-sld.normalized-scl",
    version: 1,
    source: {
      fileName: source.fileName,
      contentHash: source.contentHash,
    },
    scl: {
      version: null,
      revision: null,
    },
    dataTypeTemplates: {
      lNodeTypes: [],
      doTypes: [],
      daTypes: [],
      enumTypes: [],
    },
    substations: [],
    ieds: [],
    reportSubscriptions: [],
    diagnostics,
  }

  if (!source.xmlText.trim()) {
    diagnostics.push({
      severity: "error",
      stage: "xml",
      code: "xml.empty-source",
      message: "SCD source is empty.",
      sourceLocation: { line: 1, column: 1, offset: 0 },
    })
    return model
  }

  const topology = parseSclTopology(source.xmlText, diagnostics)
  model.scl = topology.scl
  model.dataTypeTemplates = parseSclDataTypeTemplates(source.xmlText, diagnostics)
  model.substations = topology.substations

  if (model.substations.length === 0) {
    diagnostics.push({
      severity: "error",
      stage: "parser",
      code: "parser.no-substation",
      message: "No Substation section was found in the SCD file.",
      sourceLocation: topology.firstElementLocation ?? { line: 1, column: 1, offset: 0 },
    })
  }

  model.ieds = parseIedCommunicationModel(source.xmlText, diagnostics)
  normalizeReportDataSetReferences(model)
  model.reportSubscriptions = buildIec61850ReportSubscriptionInventory(model)
  normalizeTerminalConnectivityReferences(model)

  return model
}

export { buildIec61850ReportSubscriptionInventory } from "./communicationParser"
