import type {
  SclReportControlCandidate,
  NormalizedDataLeaf,
  NormalizedDatasetEntry,
  NormalizedSclModel,
  ScdDiagnostic,
  SclBda,
  SclDa,
  SclDaType,
  SclDataSet,
  SclDataSetMember,
  SclDataTypeTemplatesModel,
  SclDo,
  SclDoType,
  SclEnumType,
  SclLNodeType,
  SclLogicalNode,
} from "./types"
import { findXmlElementRanges, readXmlAttribute, scanXmlElements, type XmlElementEvent } from "./xmlScanner"

const KNOWN_B_TYPES = new Set([
  "BOOLEAN",
  "INT8", "INT8U", "INT16", "INT16U", "INT32", "INT32U", "INT64", "INT64U",
  "FLOAT32", "FLOAT64",
  "Enum",
  "VisString255", "Unicode255", "ObjRef",
  "Quality", "Timestamp", "EntryTime",
  "Dbpos", "Check", "Octet64", "Currency",
  "Struct",
])

type TemplateFrame =
  | { kind: "LNodeType"; node: SclLNodeType }
  | { kind: "DOType"; node: SclDoType }
  | { kind: "DAType"; node: SclDaType }
  | { kind: "EnumType"; node: SclEnumType }
  | { kind: "DO"; node: SclDo }
  | { kind: "DA"; node: SclDa | SclBda }
  | { kind: "BDA"; node: SclBda }

export function parseSclDataTypeTemplates(xmlText: string, diagnostics: ScdDiagnostic[]): SclDataTypeTemplatesModel {
  const localDiagnostics: ScdDiagnostic[] = []
  const model: SclDataTypeTemplatesModel = {
    lNodeTypes: [],
    doTypes: [],
    daTypes: [],
    enumTypes: [],
  }

  for (const range of findXmlElementRanges(xmlText, ["DataTypeTemplates"], localDiagnostics)) {
    const stack: TemplateFrame[] = []
    for (const event of scanXmlElements(range.text, localDiagnostics, {
      baseOffset: range.startOffset,
      baseLine: range.sourceLocation.line,
      baseColumn: range.sourceLocation.column,
      sourcePathPrefix: ["SCL:#1", "DataTypeTemplates"],
    })) {
      if (event.kind === "close") {
        if (stack[stack.length - 1]?.kind === event.localName) {
          stack.pop()
        }
        continue
      }

      switch (event.localName) {
        case "LNodeType": {
          const node: SclLNodeType = {
            id: readXmlAttribute(event.attributes, "id")?.trim() ?? "",
            lnClass: readXmlAttribute(event.attributes, "lnClass"),
            iedType: readXmlAttribute(event.attributes, "iedType"),
            dos: [],
            sourcePath: event.sourcePath,
            sourceLocation: event.sourceLocation,
          }
          model.lNodeTypes.push(node)
          if (!event.selfClosing) stack.push({ kind: "LNodeType", node })
          break
        }
        case "DOType": {
          const node: SclDoType = {
            id: readXmlAttribute(event.attributes, "id")?.trim() ?? "",
            cdc: readXmlAttribute(event.attributes, "cdc"),
            desc: readXmlAttribute(event.attributes, "desc"),
            das: [],
            sdos: [],
            sourcePath: event.sourcePath,
            sourceLocation: event.sourceLocation,
          }
          model.doTypes.push(node)
          if (!event.selfClosing) stack.push({ kind: "DOType", node })
          break
        }
        case "DAType": {
          const node: SclDaType = {
            id: readXmlAttribute(event.attributes, "id")?.trim() ?? "",
            desc: readXmlAttribute(event.attributes, "desc"),
            bdas: [],
            sourcePath: event.sourcePath,
            sourceLocation: event.sourceLocation,
          }
          model.daTypes.push(node)
          if (!event.selfClosing) stack.push({ kind: "DAType", node })
          break
        }
        case "EnumType": {
          const node: SclEnumType = {
            id: readXmlAttribute(event.attributes, "id")?.trim() ?? "",
            desc: readXmlAttribute(event.attributes, "desc"),
            values: [],
            sourcePath: event.sourcePath,
            sourceLocation: event.sourceLocation,
          }
          model.enumTypes.push(node)
          if (!event.selfClosing) stack.push({ kind: "EnumType", node })
          break
        }
        case "DO": {
          const parent = lastFrame(stack, "LNodeType")
          if (!parent) {
            pushTemplateDiagnostic(localDiagnostics, event, "DO", "LNodeType")
            break
          }
          const node = createDo(event)
          parent.node.dos.push(node)
          if (!event.selfClosing) stack.push({ kind: "DO", node })
          break
        }
        case "SDO": {
          const parent = lastFrame(stack, "DOType")
          if (!parent) {
            pushTemplateDiagnostic(localDiagnostics, event, "SDO", "DOType")
            break
          }
          parent.node.sdos.push({
            name: readXmlAttribute(event.attributes, "name")?.trim() ?? "",
            type: readXmlAttribute(event.attributes, "type"),
            desc: readXmlAttribute(event.attributes, "desc"),
            sourcePath: event.sourcePath,
            sourceLocation: event.sourceLocation,
          })
          break
        }
        case "DA": {
          const parent = lastFrame(stack, "DOType")
          if (!parent) {
            pushTemplateDiagnostic(localDiagnostics, event, "DA", "DOType")
            break
          }
          const node = createDa(event)
          parent.node.das.push(node)
          if (!event.selfClosing) stack.push({ kind: "DA", node })
          break
        }
        case "BDA": {
          const parent = lastFrame(stack, "DAType", "DA", "BDA")
          if (!parent) {
            pushTemplateDiagnostic(localDiagnostics, event, "BDA", "DAType or DA")
            break
          }
          const node = createBda(event)
          parent.node.bdas.push(node)
          if (!event.selfClosing) stack.push({ kind: "BDA", node })
          break
        }
        case "EnumVal": {
          const parent = lastFrame(stack, "EnumType")
          if (!parent) {
            pushTemplateDiagnostic(localDiagnostics, event, "EnumVal", "EnumType")
            break
          }
          parent.node.values.push({
            value: readXmlAttribute(event.attributes, "ord")?.trim() ?? readXmlAttribute(event.attributes, "value")?.trim() ?? "",
            desc: readXmlAttribute(event.attributes, "desc"),
            sourcePath: event.sourcePath,
            sourceLocation: event.sourceLocation,
          })
          break
        }
        default:
          break
      }
    }
  }

  mergeDiagnostics(diagnostics, localDiagnostics)
  return model
}

export function normalizeIec61850DatasetEntries(input: {
  model: NormalizedSclModel
  candidate: SclReportControlCandidate
  dataSet: SclDataSet | null
}): NormalizedDatasetEntry[] {
  const { model, dataSet } = input
  if (!dataSet) {
    return []
  }

  return dataSet.members.map(member => resolveDatasetMember(model, dataSet, member))
}

function resolveDatasetMember(
  model: NormalizedSclModel,
  dataSet: SclDataSet,
  member: SclDataSetMember,
): NormalizedDatasetEntry {
  const diagnostics: ScdDiagnostic[] = []
  const context = buildMemberContext(dataSet, member)
  const sourceKind = member.kind === "FCD" ? "FCD" : "FCDA"
  const datasetRef = formatDataSetReference(dataSet)
  const logicalNode = findLogicalNodeForMember(model, dataSet, member, diagnostics)
  if (!logicalNode || !logicalNode.lnType) {
    return { datasetRef, memberRef: member.reference, leaves: [], sourceKind, diagnostics }
  }

  if (!model.dataTypeTemplates.lNodeTypes.length || !model.dataTypeTemplates.doTypes.length) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.missing-template-root", "DataTypeTemplates are unavailable.", context)
    return { datasetRef, memberRef: member.reference, leaves: [], sourceKind, diagnostics }
  }

  const lNodeType = findUniqueById(model.dataTypeTemplates.lNodeTypes, logicalNode.lnType, "LNodeType", member, diagnostics, buildMemberContext(dataSet, member, { templateKind: "LNodeType", templateId: logicalNode.lnType }))
  if (!lNodeType) {
    return { datasetRef, memberRef: member.reference, leaves: [], sourceKind, diagnostics }
  }

  if (!member.doName) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.missing-do", "DataSet member requires doName.", context)
    return { datasetRef, memberRef: member.reference, leaves: [], sourceKind, diagnostics }
  }

  const doTemplate = lNodeType.dos.find(item => item.name === member.doName)
  if (!doTemplate) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.missing-do", `Missing DO: ${member.doName}`, context)
    return { datasetRef, memberRef: member.reference, leaves: [], sourceKind, diagnostics }
  }

  const doType = doTemplate.type ? findUniqueById(model.dataTypeTemplates.doTypes, doTemplate.type, "DOType", member, diagnostics, buildMemberContext(dataSet, member, { templateKind: "DOType", templateId: doTemplate.type })) : null
  if (!doType) {
    return { datasetRef, memberRef: member.reference, leaves: [], sourceKind, diagnostics }
  }

  const pathFilter = member.daName?.trim() ? member.daName.trim().split(".").filter(Boolean) : []
  const leaves: NormalizedDataLeaf[] = []
  const fcMismatchState = { reported: false }
  const ok = expandDoType({
    model,
    logicalNode,
    dataSet,
    member,
    doName: doTemplate.name,
    doType,
    requestedFc: member.fc?.trim() || null,
    pathFilter,
    pathPrefix: [],
    leaves,
    diagnostics,
    fcMismatchState,
  })

  const hasError = diagnostics.some(diagnostic => diagnostic.severity === "error")
  if (hasError) {
    leaves.length = 0
  }

  if (!ok || leaves.length === 0) {
    if (!hasError) {
      pushMemberDiagnostic(diagnostics, member, "datatype-templates.empty-normalized-leaves", `No normalized reportable leaves were resolved for ${member.reference}.`, context)
    }
  }

  return { datasetRef, memberRef: member.reference, leaves, sourceKind, diagnostics }
}

type ExpandInput = {
  model: NormalizedSclModel
  logicalNode: SclLogicalNode
  dataSet: SclDataSet
  member: SclDataSetMember
  doName: string
  doType: SclDoType
  requestedFc: string | null
  pathPrefix: string[]
  pathFilter: string[]
  leaves: NormalizedDataLeaf[]
  diagnostics: ScdDiagnostic[]
  fcMismatchState: { reported: boolean }
}

function expandDoType(input: ExpandInput): boolean {
  let matched = false
  for (const da of input.doType.das) {
    matched = expandAttribute({
      ...input,
      attribute: da,
      pathPrefix: input.pathPrefix,
      inheritedFc: input.requestedFc,
    }) || matched
  }

  for (const sdo of input.doType.sdos) {
    const nestedDoType = sdo.type ? findUniqueById(input.model.dataTypeTemplates.doTypes, sdo.type, "DOType", input.member, input.diagnostics, buildMemberContext(input.dataSet, input.member, { templateKind: "DOType", templateId: sdo.type })) : null
    if (!nestedDoType) {
      continue
    }
    matched = expandDoType({
      ...input,
      doName: `${input.doName}.${sdo.name}`,
      doType: nestedDoType,
      pathPrefix: input.pathPrefix,
      fcMismatchState: input.fcMismatchState,
    }) || matched
  }

  return matched
}

function expandAttribute(input: {
  model: NormalizedSclModel
  logicalNode: SclLogicalNode
  dataSet: SclDataSet
  member: SclDataSetMember
  doName: string
  attribute: SclDa | SclBda
  requestedFc: string | null
  pathPrefix: string[]
  pathFilter: string[]
  inheritedFc: string | null
  leaves: NormalizedDataLeaf[]
  diagnostics: ScdDiagnostic[]
  fcMismatchState: { reported: boolean }
}): boolean {
  const { attribute, pathPrefix, pathFilter, requestedFc, inheritedFc, diagnostics, member, dataSet } = input
  const currentPath = [...pathPrefix, attribute.name]
  const context = buildMemberContext(dataSet, member, {
    bType: attribute.bType,
    count: attribute.count,
  })

  if (pathFilter.length > 0 && !matchesPathFilter(currentPath, pathFilter)) {
    return false
  }

  if (attribute.count != null && attribute.count > 1) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.array-count-not-supported", `Array expansion is not supported for count=${attribute.count} on ${member.reference}.`, context)
    return false
  }

  const bType = normalizeBType(attribute.bType, member, diagnostics, context)
  if (!bType) {
    return false
  }

  const effectiveFc = attribute.fc?.trim() || inheritedFc || requestedFc

  if (attribute.type && bType === "Struct") {
    const daType = findUniqueById(input.model.dataTypeTemplates.daTypes, attribute.type, "DAType", member, diagnostics, buildMemberContext(dataSet, member, { bType: attribute.bType, count: attribute.count, templateKind: "DAType", templateId: attribute.type }))
    if (!daType) {
      return false
    }
    let matched = false
    for (const child of daType.bdas) {
      matched = expandAttribute({
        ...input,
        attribute: child,
        pathPrefix: currentPath,
        inheritedFc: effectiveFc,
        fcMismatchState: input.fcMismatchState,
      }) || matched
    }
    return matched
  }

  if (attribute.bdas.length > 0) {
    let matched = false
    for (const child of attribute.bdas) {
      matched = expandAttribute({
        ...input,
        attribute: child,
        pathPrefix: currentPath,
        inheritedFc: effectiveFc,
        fcMismatchState: input.fcMismatchState,
      }) || matched
    }
    return matched
  }

  if (bType === "Struct") {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.unsupported-structured-member", `Unsupported structured member ${member.reference}.`, context)
    return false
  }

  if (!effectiveFc) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.missing-fc", `Missing fc for ${member.reference}.`, context)
    return false
  }
  if (requestedFc && effectiveFc !== requestedFc) {
    if (!input.fcMismatchState.reported) {
      diagnostics.push({
        severity: "warning",
        stage: "normalizer",
        code: "datatype-templates.incompatible-fc",
        message: `Incompatible fc ${requestedFc} for ${member.reference}; resolved leaf fc is ${effectiveFc}.`,
        sourcePath: member.sourcePath,
        sourceLocation: member.sourceLocation,
        context,
      })
      input.fcMismatchState.reported = true
    }
  }

  if (pathFilter.length > 0 && !isPathPrefix(pathFilter, currentPath)) {
    return false
  }

  let enumType: SclEnumType | null = null
  if (bType === "Enum") {
    if (!attribute.type) {
      pushMemberDiagnostic(diagnostics, member, "datatype-templates.missing-enumtype", `Enum attribute ${member.reference} is missing type.`, context)
      return false
    }
    enumType = findUniqueById(input.model.dataTypeTemplates.enumTypes, attribute.type, "EnumType", member, diagnostics, buildMemberContext(dataSet, member, { bType: attribute.bType, count: attribute.count, templateKind: "EnumType", templateId: attribute.type }))
    if (!enumType) {
      return false
    }
  }

  input.leaves.push({
    reference: buildLeafReference(input.logicalNode, input.doName, currentPath, effectiveFc),
    iedName: input.logicalNode.iedName,
    ldInst: input.logicalNode.logicalDeviceInst,
    prefix: input.logicalNode.prefix ?? undefined,
    lnClass: input.logicalNode.lnClass,
    lnInst: input.logicalNode.lnInst ?? undefined,
    lnType: input.logicalNode.lnType ?? "",
    doName: input.doName,
    daPath: currentPath,
    fc: effectiveFc,
    cdc: undefined,
    bType,
    type: attribute.type ?? undefined,
    enumType: enumType?.id,
    isReportable: true,
    source: {
      datasetName: input.dataSet.name,
      originalFcda: {
        reference: member.reference,
        kind: member.kind,
        sourcePath: member.sourcePath,
      },
      templateIds: [input.logicalNode.lnType ?? "", attribute.type ?? ""].filter(Boolean),
    },
  })
  return true
}

function normalizeBType(
  value: string | null,
  member: SclDataSetMember,
  diagnostics: ScdDiagnostic[],
  context: NonNullable<ScdDiagnostic["context"]>,
): string | null {
  const bType = value?.trim() ?? ""
  if (!bType) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.unknown-btype", `Unknown bType for ${member.reference}.`, context)
    return null
  }
  if (!KNOWN_B_TYPES.has(bType)) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.unknown-btype", `Unknown bType "${bType}" for ${member.reference}.`, context)
    return null
  }
  return bType
}

function findLogicalNodeForMember(
  model: NormalizedSclModel,
  dataSet: SclDataSet,
  member: SclDataSetMember,
  diagnostics: ScdDiagnostic[],
): SclLogicalNode | null {
  const context = buildMemberContext(dataSet, member)
  const logicalNodeName = member.lnClass ? formatLogicalNodeName(member.prefix, member.lnClass, member.lnInst) : dataSet.logicalNodeName
  const ldInst = member.ldInst?.trim() || dataSet.logicalDeviceInst
  const matches = collectLogicalNodes(model).filter(node =>
    node.iedName === dataSet.iedName
    && node.logicalDeviceInst === ldInst
    && node.logicalNodeName === logicalNodeName,
  )

  if (matches.length === 1) {
    return matches[0]!
  }
  if (matches.length > 1) {
    pushMemberDiagnostic(diagnostics, member, "datatype-templates.ambiguous-reference", `Ambiguous reference: ${member.reference}.`, context)
    return null
  }

  pushMemberDiagnostic(diagnostics, member, "datatype-templates.missing-logical-node", `Missing logical node: ${member.reference}.`, context)
  return null
}

function collectLogicalNodes(model: NormalizedSclModel): SclLogicalNode[] {
  return model.ieds.flatMap(ied => ied.accessPoints.flatMap(accessPoint => accessPoint.server?.logicalDevices.flatMap(logicalDevice => logicalDevice.logicalNodes) ?? []))
}

function pushMemberDiagnostic(
  diagnostics: ScdDiagnostic[],
  member: SclDataSetMember,
  code: string,
  message: string,
  context: NonNullable<ScdDiagnostic["context"]>,
): void {
  diagnostics.push({
    severity: "error",
    stage: "normalizer",
    code,
    message,
    sourcePath: member.sourcePath,
    sourceLocation: member.sourceLocation,
    context,
  })
}

function buildMemberContext(
  dataSet: SclDataSet,
  member: SclDataSetMember,
  extras: Partial<NonNullable<ScdDiagnostic["context"]>> = {},
): NonNullable<ScdDiagnostic["context"]> {
  return {
    datasetRef: formatDataSetReference(dataSet),
    memberRef: member.reference,
    iedName: dataSet.iedName,
    ldInst: member.ldInst?.trim() || dataSet.logicalDeviceInst,
    lnClass: member.lnClass,
    lnInst: member.lnInst,
    doName: member.doName,
    daName: member.daName,
    fc: member.fc,
    ...extras,
  }
}

function findUniqueById<T extends { id: string }>(
  items: readonly T[],
  id: string,
  kind: string,
  member: SclDataSetMember,
  diagnostics: ScdDiagnostic[],
  context: NonNullable<ScdDiagnostic["context"]>,
): T | null {
  const matches = items.filter(item => item.id === id)
  if (matches.length === 1) {
    return matches[0]!
  }
  if (matches.length === 0) {
    pushMemberDiagnostic(diagnostics, member, missingKindCode(kind), `Missing ${kind}: ${id}`, context)
    return null
  }
  pushMemberDiagnostic(diagnostics, member, ambiguousKindCode(kind), `Ambiguous ${kind}: ${id}`, context)
  return null
}

function missingKindCode(kind: string): string {
  switch (kind) {
    case "LNodeType":
      return "datatype-templates.missing-lnodetype"
    case "DOType":
      return "datatype-templates.missing-dotype"
    case "DAType":
      return "datatype-templates.missing-datype"
    case "EnumType":
      return "datatype-templates.unknown-enumtype"
    default:
      return `datatype-templates.missing-${kind.toLowerCase()}`
  }
}

function ambiguousKindCode(kind: string): string {
  switch (kind) {
    case "LNodeType":
      return "datatype-templates.ambiguous-lnodetype"
    case "DOType":
      return "datatype-templates.ambiguous-dotype"
    case "DAType":
      return "datatype-templates.ambiguous-datype"
    case "EnumType":
      return "datatype-templates.ambiguous-enumtype"
    default:
      return `datatype-templates.ambiguous-${kind.toLowerCase()}`
  }
}

function createDo(event: XmlElementEvent): SclDo {
  return {
    name: readXmlAttribute(event.attributes, "name")?.trim() ?? "",
    type: readXmlAttribute(event.attributes, "type"),
    desc: readXmlAttribute(event.attributes, "desc"),
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
}

function createDa(event: XmlElementEvent): SclDa {
  return {
    name: readXmlAttribute(event.attributes, "name")?.trim() ?? "",
    bType: readXmlAttribute(event.attributes, "bType"),
    type: readXmlAttribute(event.attributes, "type"),
    fc: readXmlAttribute(event.attributes, "fc"),
    count: parseCount(readXmlAttribute(event.attributes, "count")),
    desc: readXmlAttribute(event.attributes, "desc"),
    bdas: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
}

function createBda(event: XmlElementEvent): SclBda {
  return {
    name: readXmlAttribute(event.attributes, "name")?.trim() ?? "",
    bType: readXmlAttribute(event.attributes, "bType"),
    type: readXmlAttribute(event.attributes, "type"),
    fc: readXmlAttribute(event.attributes, "fc"),
    count: parseCount(readXmlAttribute(event.attributes, "count")),
    desc: readXmlAttribute(event.attributes, "desc"),
    bdas: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
}

function parseCount(value: string | null): number | null {
  if (value === null || value.trim() === "") {
    return null
  }
  const parsed = Number.parseInt(value, 10)
  return Number.isFinite(parsed) ? parsed : null
}

function formatLogicalNodeName(prefix: string | null, lnClass: string, lnInst: string | null): string {
  return `${prefix?.trim() ?? ""}${lnClass.trim()}${lnInst?.trim() ?? ""}`
}

function buildLeafReference(logicalNode: SclLogicalNode, doName: string, daPath: string[], fc: string): string {
  const ldInst = logicalNode.logicalDeviceInst
  const lnName = `${logicalNode.prefix?.trim() ?? ""}${logicalNode.lnClass.trim()}${logicalNode.lnInst?.trim() ?? ""}`
  const path = [doName, ...daPath].filter(Boolean).join(".")
  return `${ldInst}/${lnName}.${path}${fc ? `[${fc}]` : ""}`
}

function formatDataSetReference(dataSet: SclDataSet): string {
  return `${dataSet.iedName}/${dataSet.accessPointName}/${dataSet.logicalDeviceInst}/${dataSet.logicalNodeName}.${dataSet.name}`
}

function isPathPrefix(prefix: string[], path: string[]): boolean {
  if (prefix.length > path.length) {
    return false
  }
  return prefix.every((part, index) => part === path[index])
}

function matchesPathFilter(currentPath: string[], pathFilter: string[]): boolean {
  return isPathPrefix(currentPath, pathFilter) || isPathPrefix(pathFilter, currentPath)
}

function lastFrame<T extends TemplateFrame["kind"]>(stack: TemplateFrame[], ...kinds: T[]): Extract<TemplateFrame, { kind: T }> | null {
  for (let index = stack.length - 1; index >= 0; index -= 1) {
    const frame = stack[index]
    if (frame && kinds.includes(frame.kind as T)) {
      return frame as Extract<TemplateFrame, { kind: T }>
    }
  }
  return null
}

function pushTemplateDiagnostic(
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
  child: string,
  parent: string,
) {
  diagnostics.push({
    severity: "error",
    stage: "normalizer",
    code: "template.missing-parent",
    message: `${child} is outside ${parent}; it was skipped.`,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  })
}

function mergeDiagnostics(target: ScdDiagnostic[], source: ScdDiagnostic[]) {
  const seen = new Set(target.map(diagnostic => fingerprint(diagnostic)))
  for (const diagnostic of source) {
    const key = fingerprint(diagnostic)
    if (seen.has(key)) continue
    seen.add(key)
    target.push(diagnostic)
  }
}

function fingerprint(diagnostic: ScdDiagnostic): string {
  return [
    diagnostic.severity,
    diagnostic.stage,
    diagnostic.code,
    diagnostic.sourceId ?? "",
    diagnostic.sourcePath ?? "",
    diagnostic.sourceLocation?.line ?? "",
    diagnostic.sourceLocation?.column ?? "",
    diagnostic.message,
  ].join("\u0000")
}
