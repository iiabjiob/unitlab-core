import type {
  Iec61850ReportControlCandidate,
  Iec61850ReportSubscriptionPlan,
  Iec61850ReportSubscriptionPlanDiagnostic,
  Iec61850ReportSubscriptionPlanSignal,
  Iec61850SelectedSignal,
} from "./types"
import { getIec61850ReportCandidateSignals } from "./normalizedSignals"

type BuildPlanOptions = {
  candidates: readonly Iec61850ReportControlCandidate[]
  selectedSignals: readonly Iec61850SelectedSignal[]
}

type ModelSignal = {
  key: string
  iedName: string
  reference: string
  reportCandidateIds: Set<string>
}

type SignalIndex = {
  byVariant: Map<string, Set<ModelSignal>>
  reportsById: Map<string, Iec61850ReportControlCandidate>
  ldInsts: Set<string>
}

type MatchResult =
  | { kind: "none" }
  | { kind: "matched"; signal: ModelSignal; matchKind: Iec61850ReportSubscriptionPlanSignal["matchKind"] }
  | { kind: "ambiguous"; signals: ModelSignal[] }

type ParsedReference = {
  ldInst: string | null
  logicalNodeName: string
  dataPath: string[]
  fcSuffix: string
  ixSuffix: string
}

type ReferenceVariantContext = {
  iedName?: string | null
  knownLdInsts?: ReadonlySet<string>
  includeParentPaths?: boolean
  stripFunctionalConstraint?: boolean
}

export function buildIec61850ReportSubscriptionPlan(
  options: BuildPlanOptions,
): Iec61850ReportSubscriptionPlan {
  const index = buildSignalIndex(options.candidates)
  const diagnostics: Iec61850ReportSubscriptionPlanDiagnostic[] = []
  const matchedSignals: Iec61850ReportSubscriptionPlanSignal[] = []
  const unmatchedSignals: Iec61850SelectedSignal[] = []
  const ambiguousSignals: Iec61850ReportSubscriptionPlan["ambiguousSignals"] = []
  const reportMatches = new Map<string, Iec61850ReportSubscriptionPlanSignal[]>()
  const seenSelectedAddresses = new Map<string, Iec61850SelectedSignal>()

  for (const selectedSignal of options.selectedSignals) {
    const duplicateKey = duplicateSignalKey(selectedSignal.address)
    const previous = seenSelectedAddresses.get(duplicateKey)
    if (previous) {
      diagnostics.push({
        severity: "warning",
        code: "DUPLICATE_SELECTED_SIGNAL",
        message: `Selected signal "${selectedSignal.address}" duplicates "${previous.address}".`,
        signalId: selectedSignal.id,
        address: selectedSignal.address,
      })
    } else {
      seenSelectedAddresses.set(duplicateKey, selectedSignal)
    }

    const match = matchSelectedSignal(index, selectedSignal.address)
    if (match.kind === "none") {
      unmatchedSignals.push(selectedSignal)
      diagnostics.push({
        severity: "error",
        code: "SIGNAL_NOT_FOUND",
        message: `Selected IEC 61850 signal "${selectedSignal.address}" was not found in SCD report DataSets.`,
        signalId: selectedSignal.id,
        address: selectedSignal.address,
      })
      continue
    }

    if (match.kind === "ambiguous") {
      ambiguousSignals.push({
        selectedSignal,
        candidates: match.signals.map(signal => ({
          iedName: signal.iedName,
          reference: signal.reference,
          reportCandidateIds: Array.from(signal.reportCandidateIds).sort(),
        })),
      })
      diagnostics.push({
        severity: "error",
        code: "SIGNAL_AMBIGUOUS",
        message: `Selected IEC 61850 signal "${selectedSignal.address}" matches ${match.signals.length} SCD signals.`,
        signalId: selectedSignal.id,
        address: selectedSignal.address,
      })
      continue
    }

    const planSignal: Iec61850ReportSubscriptionPlanSignal = {
      selectedSignal,
      modelReference: match.signal.reference,
      iedName: match.signal.iedName,
      matchKind: match.matchKind,
    }
    matchedSignals.push(planSignal)

    if (match.matchKind === "fcd-parent") {
      diagnostics.push({
        severity: "info",
        code: "FCD_PARENT_MATCH",
        message: `Selected IEC 61850 signal "${selectedSignal.address}" matched a parent FCD DataSet member "${match.signal.reference}".`,
        signalId: selectedSignal.id,
        address: selectedSignal.address,
      })
    }
    if (match.signal.reportCandidateIds.size > 1) {
      diagnostics.push({
        severity: "warning",
        code: "MULTIPLE_REPORT_CANDIDATES",
        message: `Selected IEC 61850 signal "${selectedSignal.address}" is available through ${match.signal.reportCandidateIds.size} report candidates.`,
        signalId: selectedSignal.id,
        address: selectedSignal.address,
      })
    }

    for (const reportCandidateId of match.signal.reportCandidateIds) {
      const bucket = reportMatches.get(reportCandidateId) ?? []
      bucket.push(planSignal)
      reportMatches.set(reportCandidateId, bucket)
    }
  }

  const devices = Array.from(reportMatches.entries())
    .map(([reportCandidateId, signals]) => {
      const candidate = index.reportsById.get(reportCandidateId)
      return candidate ? { candidate, signals } : null
    })
    .filter((item): item is { candidate: Iec61850ReportControlCandidate; signals: Iec61850ReportSubscriptionPlanSignal[] } => Boolean(item))
    .sort((left, right) => compareReportCandidates(left.candidate, right.candidate))
    .reduce<Iec61850ReportSubscriptionPlan["devices"]>((acc, item) => {
      const device = findOrCreateDevice(acc, item.candidate)
      device.reports.push({
        status: "required",
        candidate: item.candidate,
        matchedSignals: item.signals.sort(comparePlanSignals),
      })
      return acc
    }, [])

  devices.forEach(device => device.reports.sort((left, right) => compareReportCandidates(left.candidate, right.candidate)))

  return {
    selectedSignalCount: options.selectedSignals.length,
    matchedSignalCount: matchedSignals.length,
    unmatchedSignalCount: unmatchedSignals.length,
    ambiguousSignalCount: ambiguousSignals.length,
    requiredReportCount: reportMatches.size,
    devices,
    matchedSignals: matchedSignals.sort(comparePlanSignals),
    unmatchedSignals: [...unmatchedSignals].sort(compareSelectedSignals),
    ambiguousSignals,
    diagnostics,
  }
}

function buildSignalIndex(candidates: readonly Iec61850ReportControlCandidate[]): SignalIndex {
  const ldInsts = collectKnownLdInsts(candidates)
  const index: SignalIndex = {
    byVariant: new Map(),
    reportsById: new Map(),
    ldInsts,
  }
  const signalsByKey = new Map<string, ModelSignal>()

  for (const candidate of candidates) {
    index.reportsById.set(candidate.id, candidate)
    for (const signal of getIec61850ReportCandidateSignals(candidate)) {
      const key = `${normalizeReference(candidate.iedName)}\u0000${normalizeReference(signal.reference)}`
      const modelSignal = signalsByKey.get(key) ?? {
        key,
        iedName: candidate.iedName,
        reference: signal.reference,
        reportCandidateIds: new Set<string>(),
      }
      modelSignal.reportCandidateIds.add(candidate.id)
      signalsByKey.set(key, modelSignal)
      for (const variant of referenceVariants(signal.reference, {
        iedName: candidate.iedName,
        knownLdInsts: ldInsts,
      })) {
        addVariant(index, variant, modelSignal)
      }
    }
  }

  return index
}

function matchSelectedSignal(index: SignalIndex, address: string): MatchResult {
  const exact = collectCandidates(index, address, false)
  if (exact.size === 1) {
    return { kind: "matched", signal: Array.from(exact)[0]!, matchKind: "exact" }
  }
  if (exact.size > 1) {
    return { kind: "ambiguous", signals: Array.from(exact).sort(compareModelSignals) }
  }

  const parent = collectCandidates(index, address, true)
  if (parent.size === 1) {
    return { kind: "matched", signal: Array.from(parent)[0]!, matchKind: "fcd-parent" }
  }
  if (parent.size > 1) {
    return { kind: "ambiguous", signals: Array.from(parent).sort(compareModelSignals) }
  }
  return { kind: "none" }
}

function collectCandidates(index: SignalIndex, address: string, includeParentPaths: boolean): Set<ModelSignal> {
  const stripFunctionalConstraint = !referenceHasFunctionalConstraint(address)
  for (const variant of referenceVariants(address, {
    knownLdInsts: index.ldInsts,
    includeParentPaths,
    stripFunctionalConstraint,
  })) {
    const bucket = index.byVariant.get(variant)
    if (bucket?.size) {
      return new Set(bucket)
    }
  }
  return new Set()
}

function collectKnownLdInsts(candidates: readonly Iec61850ReportControlCandidate[]): Set<string> {
  const ldInsts = new Set<string>()
  for (const candidate of candidates) {
    addNormalized(ldInsts, candidate.logicalDeviceInst)
    for (const signal of getIec61850ReportCandidateSignals(candidate)) {
      addNormalized(ldInsts, parseReference(normalizeReference(signal.reference))?.ldInst)
    }
    const dataSetRefParts = normalizeReference(candidate.dataSetRef ?? "").split("/").filter(Boolean)
    addNormalized(ldInsts, dataSetRefParts[2])
  }
  return ldInsts
}

function addVariant(index: SignalIndex, variant: string, signal: ModelSignal) {
  const bucket = index.byVariant.get(variant) ?? new Set<ModelSignal>()
  bucket.add(signal)
  index.byVariant.set(variant, bucket)
}

function addNormalized(target: Set<string>, value: string | null | undefined) {
  const normalized = normalizeReference(value ?? "")
  if (normalized) target.add(normalized)
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

function duplicateSignalKey(address: string): string {
  const sources = normalizeReferenceSources(address)
  const source = sources[sources.length - 1] ?? normalizeReference(address)
  const parsed = parseReference(source)
  if (!parsed) return source
  const prefix = parsed.ldInst ? `${parsed.ldInst}/` : ""
  return `${prefix}${parsed.logicalNodeName}/${parsed.dataPath.join("/")}${parsed.fcSuffix}${parsed.ixSuffix}`
}

function findOrCreateDevice(
  devices: Iec61850ReportSubscriptionPlan["devices"],
  candidate: Iec61850ReportControlCandidate,
) {
  const existing = devices.find(device => (
    device.iedName === candidate.iedName
    && device.accessPointName === candidate.accessPointName
  ))
  if (existing) return existing

  const created = {
    iedName: candidate.iedName,
    accessPointName: candidate.accessPointName,
    reports: [],
  }
  devices.push(created)
  devices.sort(compareDevices)
  return created
}

function compareDevices(
  left: Iec61850ReportSubscriptionPlan["devices"][number],
  right: Iec61850ReportSubscriptionPlan["devices"][number],
): number {
  return `${left.iedName}/${left.accessPointName}`.localeCompare(`${right.iedName}/${right.accessPointName}`)
}

function compareReportCandidates(
  left: Iec61850ReportControlCandidate,
  right: Iec61850ReportControlCandidate,
): number {
  return [
    left.iedName,
    left.accessPointName,
    left.logicalDeviceInst,
    left.logicalNodeName,
    left.reportControlName,
    left.reportKind,
  ].join("/").localeCompare([
    right.iedName,
    right.accessPointName,
    right.logicalDeviceInst,
    right.logicalNodeName,
    right.reportControlName,
    right.reportKind,
  ].join("/"))
}

function comparePlanSignals(
  left: Iec61850ReportSubscriptionPlanSignal,
  right: Iec61850ReportSubscriptionPlanSignal,
): number {
  return `${left.iedName}/${left.modelReference}/${left.selectedSignal.id}`
    .localeCompare(`${right.iedName}/${right.modelReference}/${right.selectedSignal.id}`)
}

function compareSelectedSignals(left: Iec61850SelectedSignal, right: Iec61850SelectedSignal): number {
  return `${left.address}/${left.id}`.localeCompare(`${right.address}/${right.id}`)
}

function compareModelSignals(left: ModelSignal, right: ModelSignal): number {
  return `${left.iedName}/${left.reference}`.localeCompare(`${right.iedName}/${right.reference}`)
}
