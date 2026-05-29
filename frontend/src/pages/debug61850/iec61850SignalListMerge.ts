import type { SignalAllocationRow, SignalSheet } from "@/types/signal"
import {
  extractSourceRowFromSignalMetadata,
  resolveAllSourceColumnHeaders,
} from "@/pages/signals/utils/sourceColumns"
import type { Iec61850DebugDocument } from "./iec61850DebugTree"

export type Iec61850SignalListMergeMatch = {
  signalId: number
  signalKey: string
  signalName: string
  address: string
  modelReference: string
  dataSets: string[]
  reports: Iec61850SignalListMergeReport[]
  ieds: string[]
}

export type Iec61850SignalListMergeMiss = {
  signalId: number
  signalKey: string
  signalName: string
  address: string
}

export type Iec61850SignalListMergeReport = {
  name: string
  kind: string
  dataSetRef: string | null
  iedName: string | null
}

export type Iec61850SignalListMergeResult = {
  rowCount: number
  addressColumn: string | null
  columnConfidence: "high" | "medium" | "none"
  addressRows: number
  matchedRows: number
  unmatchedRows: number
  rowsWithoutAddress: number
  matchedReports: Iec61850SignalListMergeReport[]
  matchedIeds: string[]
  matches: Iec61850SignalListMergeMatch[]
  misses: Iec61850SignalListMergeMiss[]
}

type MergeModelSignal = {
  reference: string
  dataSets: Set<string>
  reports: Map<string, Iec61850SignalListMergeReport>
  ieds: Set<string>
}

type RowMergeContext = {
  sourceRow: Record<string, unknown>
  sourceValues: string[]
  iedHints: Set<string>
}

type ReferenceVariantContext = {
  iedName?: string | null
  knownLdInsts?: ReadonlySet<string>
  includeParentPaths?: boolean
  stripFunctionalConstraint?: boolean
}

type ParsedReference = {
  ldInst: string | null
  logicalNodeName: string
  dataPath: string[]
  fcSuffix: string
  ixSuffix: string
}

type ColumnCandidate = {
  header: string
  nonEmpty: number
  exactMatches: number
  patternMatches: number
  score: number
}

type MergeIndex = {
  byReference: Map<string, MergeModelSignal>
  byKey: Map<string, Set<MergeModelSignal>>
  iedNames: Set<string>
  ldInsts: Set<string>
}

const PREFERRED_FULL_61850_PATH_HEADER = "Full 61850 Path"

export function mergeIec61850SignalList(
  document: Iec61850DebugDocument,
  rows: readonly SignalAllocationRow[],
  sheet: SignalSheet | null,
): Iec61850SignalListMergeResult {
  const index = buildMergeIndex(document)
  const rowContexts = rows.map(row => buildRowMergeContext(row, index))
  const headers = resolveAllSourceColumnHeaders(sheet, rows)
  const column = resolvePreferredFullPathColumn(rowContexts, headers, index)
    ?? detectIec61850AddressColumn(rowContexts, headers, index)

  if (!column) {
    return emptyMergeResult(rows.length)
  }

  const matches: Iec61850SignalListMergeMatch[] = []
  const misses: Iec61850SignalListMergeMiss[] = []
  let rowsWithoutAddress = 0

  for (const [rowIndex, row] of rows.entries()) {
    const context = rowContexts[rowIndex]
    const address = normalizeDisplayValue(context?.sourceRow[column.header])
    if (!address) {
      rowsWithoutAddress += 1
      continue
    }

    const modelSignal = resolveModelSignal(index, address, context)
    if (!modelSignal) {
      misses.push({
        signalId: row.signal_id,
        signalKey: row.signal_key,
        signalName: row.signal_name,
        address,
      })
      continue
    }

    matches.push({
      signalId: row.signal_id,
      signalKey: row.signal_key,
      signalName: row.signal_name,
      address,
      modelReference: modelSignal.reference,
      dataSets: Array.from(modelSignal.dataSets).sort(),
      reports: Array.from(modelSignal.reports.values()).sort(compareReports),
      ieds: Array.from(modelSignal.ieds).sort(),
    })
  }

  const reportMap = new Map<string, Iec61850SignalListMergeReport>()
  const ieds = new Set<string>()
  for (const match of matches) {
    match.reports.forEach((report) => reportMap.set(reportKey(report), report))
    match.ieds.forEach(ied => ieds.add(ied))
  }

  const addressRows = matches.length + misses.length
  return {
    rowCount: rows.length,
    addressColumn: column.header,
    columnConfidence: column.exactMatches > 0 ? "high" : "medium",
    addressRows,
    matchedRows: matches.length,
    unmatchedRows: misses.length,
    rowsWithoutAddress,
    matchedReports: Array.from(reportMap.values()).sort(compareReports),
    matchedIeds: Array.from(ieds).sort(),
    matches,
    misses,
  }
}

function emptyMergeResult(rowCount: number): Iec61850SignalListMergeResult {
  return {
    rowCount,
    addressColumn: null,
    columnConfidence: "none",
    addressRows: 0,
    matchedRows: 0,
    unmatchedRows: 0,
    rowsWithoutAddress: rowCount,
    matchedReports: [],
    matchedIeds: [],
    matches: [],
    misses: [],
  }
}

function detectIec61850AddressColumn(
  rowContexts: readonly RowMergeContext[],
  headers: readonly string[],
  index: MergeIndex,
): ColumnCandidate | null {
  const candidates = headers.map(header => buildColumnCandidate(rowContexts, header, index))

  const [best] = candidates
    .filter(candidate => candidate.nonEmpty > 0)
    .sort((left, right) => right.score - left.score)

  if (!best) return null
  if (best.exactMatches > 0) return best
  if (best.patternMatches >= Math.max(2, Math.ceil(best.nonEmpty * 0.2))) return best
  return null
}

function resolvePreferredFullPathColumn(
  rowContexts: readonly RowMergeContext[],
  headers: readonly string[],
  index: MergeIndex,
): ColumnCandidate | null {
  const header = headers.find(item => (
    item.trim().toLowerCase() === PREFERRED_FULL_61850_PATH_HEADER.toLowerCase()
  ))
  if (!header) return null

  const candidate = buildColumnCandidate(rowContexts, header, index)
  return candidate.nonEmpty > 0 ? candidate : null
}

function buildColumnCandidate(
  rowContexts: readonly RowMergeContext[],
  header: string,
  index: MergeIndex,
): ColumnCandidate {
  let nonEmpty = 0
  let exactMatches = 0
  let patternMatches = 0

  for (const context of rowContexts) {
    const value = normalizeDisplayValue(context.sourceRow[header])
    if (!value) continue
    nonEmpty += 1
    if (resolveModelSignal(index, value, context)) {
      exactMatches += 1
    }
    if (looksLikeIec61850Address(value)) {
      patternMatches += 1
    }
  }

  return {
    header,
    nonEmpty,
    exactMatches,
    patternMatches,
    score: exactMatches * 20 + patternMatches * 4 + headerNameScore(header),
  }
}

function buildMergeIndex(document: Iec61850DebugDocument): MergeIndex {
  const index: MergeIndex = {
    byReference: new Map<string, MergeModelSignal>(),
    byKey: new Map<string, Set<MergeModelSignal>>(),
    iedNames: new Set<string>(),
    ldInsts: collectKnownLogicalDeviceInsts(document),
  }

  for (const item of document.signalInventory.dataSetSignals) {
    const signal = ensureModelSignal(index, item.reference, { iedName: item.iedName })
    signal.dataSets.add(item.dataSetRef)
    signal.ieds.add(item.iedName)
  }

  for (const item of document.signalInventory.reportSignals) {
    const signal = ensureModelSignal(index, item.reference, { iedName: item.iedName })
    if (item.dataSetRef) {
      signal.dataSets.add(item.dataSetRef)
    }
    signal.ieds.add(item.iedName)
    const report = {
      name: item.reportControlName,
      kind: item.reportKind,
      dataSetRef: item.dataSetRef,
      iedName: item.iedName,
    }
    signal.reports.set(reportKey(report), report)
  }

  return index
}

function ensureModelSignal(
  index: MergeIndex,
  reference: string,
  context: ReferenceVariantContext = {},
): MergeModelSignal {
  const canonicalReference = modelSignalKey(reference, context)
  const existing = index.byReference.get(canonicalReference)
  if (existing) return existing

  const normalizedIedName = normalizeReference(context.iedName ?? "")
  if (normalizedIedName) {
    index.iedNames.add(normalizedIedName)
  }

  const created: MergeModelSignal = {
    reference,
    dataSets: new Set<string>(),
    reports: new Map<string, Iec61850SignalListMergeReport>(),
    ieds: new Set<string>(),
  }
  index.byReference.set(canonicalReference, created)
  referenceVariants(reference, {
    ...context,
    knownLdInsts: index.ldInsts,
  }).forEach((variant) => addSignalVariant(index, variant, created))
  return created
}

function collectKnownLogicalDeviceInsts(document: Iec61850DebugDocument): Set<string> {
  const ldInsts = new Set<string>()
  const addLdInst = (value: string | null | undefined) => {
    const normalized = normalizeReference(value ?? "")
    if (normalized) {
      ldInsts.add(normalized)
    }
  }

  const collectFromDataSetRef = (dataSetRef: string | null) => {
    const parts = normalizeReference(dataSetRef ?? "").split("/").filter(Boolean)
    addLdInst(parts[2])
  }

  document.signalInventory.dataSetSignals.forEach((item) => {
    addLdInst(parseReference(normalizeReference(item.reference))?.ldInst)
    collectFromDataSetRef(item.dataSetRef)
  })
  document.signalInventory.reportSignals.forEach((item) => {
    addLdInst(parseReference(normalizeReference(item.reference))?.ldInst)
    collectFromDataSetRef(item.dataSetRef)
  })

  return ldInsts
}

function buildRowMergeContext(row: SignalAllocationRow, index: MergeIndex): RowMergeContext {
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const sourceValues = [
    ...Object.values(sourceRow),
    row.signal_key,
    row.signal_name,
  ]
    .map(normalizeDisplayValue)
    .filter(Boolean)
  const normalizedValues = sourceValues.map(normalizeReference).filter(Boolean)
  const iedHints = new Set<string>()

  for (const iedName of index.iedNames) {
    if (normalizedValues.some(value => value.includes(iedName))) {
      iedHints.add(iedName)
    }
  }

  return {
    sourceRow,
    sourceValues,
    iedHints,
  }
}

function addSignalVariant(index: MergeIndex, variant: string, signal: MergeModelSignal) {
  const bucket = index.byKey.get(variant) ?? new Set<MergeModelSignal>()
  bucket.add(signal)
  index.byKey.set(variant, bucket)
}

function resolveModelSignal(
  index: MergeIndex,
  address: string,
  context?: RowMergeContext,
): MergeModelSignal | null {
  const candidates = collectModelSignalCandidates(index, address)
  if (candidates.size === 0) return null
  if (candidates.size === 1) {
    return Array.from(candidates)[0] ?? null
  }

  const contextCandidate = resolveAmbiguousModelSignal(index, candidates, address, context)
  if (contextCandidate) return contextCandidate

  return mergeModelSignalCandidates(address, candidates)
}

function mergeModelSignalCandidates(address: string, candidates: Set<MergeModelSignal>): MergeModelSignal {
  const candidateList = Array.from(candidates)
  const reports = new Map<string, Iec61850SignalListMergeReport>()
  const dataSets = new Set<string>()
  const ieds = new Set<string>()

  candidateList.forEach((candidate) => {
    candidate.dataSets.forEach(dataSet => dataSets.add(dataSet))
    candidate.reports.forEach(report => reports.set(reportKey(report), report))
    candidate.ieds.forEach(iedName => ieds.add(iedName))
  })

  return {
    reference: `${candidateList.length} possible SCD signals for ${address}`,
    dataSets,
    reports,
    ieds,
  }
}

function collectModelSignalCandidates(index: MergeIndex, address: string): Set<MergeModelSignal> {
  const exactCandidates = collectCandidatesForReferenceVariants(index, referenceVariants(address, {
    knownLdInsts: index.ldInsts,
    stripFunctionalConstraint: !referenceHasFunctionalConstraint(address),
  }))
  if (exactCandidates.size > 0) {
    return exactCandidates
  }

  return collectCandidatesForReferenceVariants(index, referenceVariants(address, {
    knownLdInsts: index.ldInsts,
    includeParentPaths: true,
    stripFunctionalConstraint: !referenceHasFunctionalConstraint(address),
  }))
}

function collectCandidatesForReferenceVariants(
  index: MergeIndex,
  variants: readonly string[],
): Set<MergeModelSignal> {
  const candidates = new Set<MergeModelSignal>()
  for (const key of variants) {
    const signals = index.byKey.get(key)
    signals?.forEach(signal => candidates.add(signal))
  }
  return candidates
}

function resolveAmbiguousModelSignal(
  index: MergeIndex,
  candidates: Set<MergeModelSignal>,
  address: string,
  context?: RowMergeContext,
): MergeModelSignal | null {
  if (!context) return null

  const addressKey = normalizeReference(address)
  for (const value of context.sourceValues) {
    if (normalizeReference(value) === addressKey) continue
    const valueCandidates = collectModelSignalCandidates(index, value)
    const scopedCandidates = intersectCandidates(candidates, valueCandidates)
    if (scopedCandidates.size === 1) {
      return Array.from(scopedCandidates)[0] ?? null
    }
  }

  if (context.iedHints.size > 0) {
    const scopedCandidates = new Set(
      Array.from(candidates).filter(candidate => (
        Array.from(candidate.ieds).some(iedName => context.iedHints.has(normalizeReference(iedName)))
      )),
    )
    if (scopedCandidates.size === 1) {
      return Array.from(scopedCandidates)[0] ?? null
    }
    if (scopedCandidates.size > 1) {
      return mergeModelSignalCandidates(address, scopedCandidates)
    }
  }

  return null
}

function intersectCandidates(
  left: Set<MergeModelSignal>,
  right: Set<MergeModelSignal>,
): Set<MergeModelSignal> {
  const result = new Set<MergeModelSignal>()
  right.forEach((candidate) => {
    if (left.has(candidate)) {
      result.add(candidate)
    }
  })
  return result
}

function referenceVariants(value: string, context: ReferenceVariantContext = {}): string[] {
  const variants = new Set<string>()
  const stripFc = context.stripFunctionalConstraint ?? true

  for (const source of normalizeReferenceSources(value)) {
    addReferenceVariant(variants, source, stripFc)

    const parsed = parseReference(source)
    if (!parsed) continue

    referenceLdInstCandidates(parsed.ldInst, context).forEach((ldInst) => {
      addParsedReferenceVariants(variants, { ...parsed, ldInst }, {
        includeParentPaths: context.includeParentPaths === true,
        stripFunctionalConstraint: stripFc,
      })
    })

    addParsedReferenceVariants(variants, { ...parsed, ldInst: null }, {
      includeParentPaths: context.includeParentPaths === true,
      stripFunctionalConstraint: stripFc,
    })
  }

  return Array.from(variants).filter(Boolean)
}

function normalizeReferenceSources(value: string): string[] {
  const normalized = normalizeReference(value)
  if (!normalized) return []

  const values = new Set([normalized])
  const bangIndex = normalized.lastIndexOf("!")
  if (bangIndex >= 0 && bangIndex < normalized.length - 1) {
    values.add(normalized.slice(bangIndex + 1))
  }
  return Array.from(values)
}

function addReferenceVariant(variants: Set<string>, value: string, stripFc = true) {
  variants.add(value)
  if (stripFc) {
    variants.add(stripFunctionalConstraint(value))
  }
}

function referenceLdInstCandidates(ldInst: string | null, context: ReferenceVariantContext): string[] {
  if (!ldInst) return []

  const candidates = new Set([ldInst])
  context.knownLdInsts?.forEach((knownLdInst) => {
    if (knownLdInst && ldInst !== knownLdInst && ldInst.endsWith(knownLdInst)) {
      candidates.add(knownLdInst)
    }
  })
  const iedName = normalizeReference(context.iedName ?? "")
  if (iedName) {
    candidates.add(`${iedName}${ldInst}`)
    if (ldInst.startsWith(iedName) && ldInst.length > iedName.length) {
      candidates.add(ldInst.slice(iedName.length))
    }
  }
  return Array.from(candidates)
}

function addParsedReferenceVariants(
  variants: Set<string>,
  reference: ParsedReference,
  options: {
    includeParentPaths: boolean
    stripFunctionalConstraint: boolean
  },
) {
  if (!reference.logicalNodeName || !reference.dataPath.length) return

  const prefix = reference.ldInst ? `${reference.ldInst}/` : ""
  const suffix = `${reference.fcSuffix}${reference.ixSuffix}`
  addDataPathReferenceVariants(
    variants,
    prefix,
    reference.logicalNodeName,
    reference.dataPath,
    suffix,
    options.stripFunctionalConstraint,
  )

  if (!options.includeParentPaths) return
  for (let end = reference.dataPath.length - 1; end >= 1; end -= 1) {
    addDataPathReferenceVariants(
      variants,
      prefix,
      reference.logicalNodeName,
      reference.dataPath.slice(0, end),
      suffix,
      options.stripFunctionalConstraint,
    )
  }
}

function addDataPathReferenceVariants(
  variants: Set<string>,
  prefix: string,
  logicalNodeName: string,
  dataPath: readonly string[],
  suffix: string,
  stripFc: boolean,
) {
  const dotted = `${prefix}${logicalNodeName}.${dataPath.join(".")}${suffix}`
  addReferenceVariant(variants, dotted, stripFc)

  const [firstDataPart, ...restDataParts] = dataPath
  if (firstDataPart) {
    const groupedDataPath = restDataParts.length
      ? `${firstDataPart}/${restDataParts.join(".")}`
      : firstDataPart
    addReferenceVariant(variants, `${prefix}${logicalNodeName}/${groupedDataPath}${suffix}`, stripFc)
  }

  addReferenceVariant(variants, `${prefix}${logicalNodeName}/${dataPath.join("/")}${suffix}`, stripFc)
}

function parseReference(value: string): ParsedReference | null {
  const { body, fcSuffix, ixSuffix } = splitReferenceSuffix(value)
  const parts = body.split("/").map(part => part.trim()).filter(Boolean)
  if (!parts.length) return null

  let ldInst: string | null = null
  let logicalNodeName = ""
  let dataPath: string[] = []

  if (parts.length === 1) {
    const parsed = splitLogicalNodeAndData(parts[0])
    if (!parsed) return null
    logicalNodeName = parsed.logicalNodeName
    dataPath = parsed.dataPath
  } else {
    ldInst = parts[0] ?? null
    const parsed = splitLogicalNodeAndData(parts[1] ?? "")
    if (parsed) {
      logicalNodeName = parsed.logicalNodeName
      dataPath = [...parsed.dataPath, ...splitDataPathParts(parts.slice(2))]
    } else {
      logicalNodeName = parts[1] ?? ""
      dataPath = splitDataPathParts(parts.slice(2))
    }
  }

  if (!logicalNodeName || !dataPath.length) return null
  return {
    ldInst,
    logicalNodeName,
    dataPath,
    fcSuffix,
    ixSuffix,
  }
}

function splitLogicalNodeAndData(value: string): { logicalNodeName: string; dataPath: string[] } | null {
  const dotIndex = value.indexOf(".")
  if (dotIndex <= 0 || dotIndex >= value.length - 1) return null
  return {
    logicalNodeName: value.slice(0, dotIndex),
    dataPath: splitDataPathParts([value.slice(dotIndex + 1)]),
  }
}

function splitDataPathParts(parts: readonly string[]): string[] {
  return parts
    .flatMap(part => part.split("."))
    .map(part => part.trim())
    .filter(Boolean)
}

function normalizeReference(value: string): string {
  return String(value ?? "")
    .trim()
    .replace(/\\/g, "/")
    .replace(/\s+/g, "")
    .toLowerCase()
}

function stripFunctionalConstraint(value: string): string {
  const { body, ixSuffix } = splitReferenceSuffix(value)
  return `${body}${ixSuffix}`
}

function splitReferenceSuffix(value: string): { body: string; fcSuffix: string; ixSuffix: string } {
  let body = value
  let ixSuffix = ""
  const ixMatch = body.match(/#[^#]+$/u)
  if (ixMatch) {
    ixSuffix = ixMatch[0]
    body = body.slice(0, -ixSuffix.length)
  }

  let fcSuffix = ""
  const fcMatch = body.match(/\[[^\]]+\]$/u)
  if (fcMatch) {
    fcSuffix = fcMatch[0]
    body = body.slice(0, -fcSuffix.length)
  }

  return { body, fcSuffix, ixSuffix }
}

function referenceHasFunctionalConstraint(value: string): boolean {
  return normalizeReferenceSources(value).some(source => splitReferenceSuffix(source).fcSuffix !== "")
}

function modelSignalKey(reference: string, context: ReferenceVariantContext): string {
  return `${normalizeReference(context.iedName ?? "")}\u0000${normalizeReference(reference)}`
}

function looksLikeIec61850Address(value: string): boolean {
  return normalizeReferenceSources(value).some((source) => {
    const hasFunctionalConstraint = /\[[a-z]{2}\](?:#[^#]+)?$/u.test(source)
    if (!hasFunctionalConstraint) return false
    return parseReference(source) !== null
  })
}

function headerNameScore(header: string): number {
  return /61850|iec|mms|fcda|адрес|address|addr|ref/i.test(header) ? 3 : 0
}

function normalizeDisplayValue(value: unknown): string {
  return String(value ?? "").trim()
}

function reportKey(report: Iec61850SignalListMergeReport): string {
  return `${report.iedName ?? ""}:${report.name}:${report.kind}:${report.dataSetRef ?? ""}`
}

function compareReports(left: Iec61850SignalListMergeReport, right: Iec61850SignalListMergeReport): number {
  return reportKey(left).localeCompare(reportKey(right))
}
