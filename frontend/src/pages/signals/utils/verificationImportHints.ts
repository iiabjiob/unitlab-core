export type VerificationImportHintConfidence = "exact" | "likely" | "possible" | "missing"

export interface VerificationImportColumn {
  header: string
  index: number
}

export interface VerificationImportFieldHint {
  column: string | null
  confidence: VerificationImportHintConfidence
  reason: string
  sample_values: string[]
}

export interface VerificationImportHints {
  transport_reference_column_hint: VerificationImportFieldHint
  transport_host_column_hint: VerificationImportFieldHint
  transport_port_column_hint: VerificationImportFieldHint
  ied_name_column_hint: VerificationImportFieldHint
  access_point_name_column_hint: VerificationImportFieldHint
  iec61850_address_column_hint: VerificationImportFieldHint
  logical_device_inst_column_hint: VerificationImportFieldHint
  logical_node_name_column_hint: VerificationImportFieldHint
  data_set_reference_column_hint: VerificationImportFieldHint
  report_control_reference_column_hint: VerificationImportFieldHint
  notes: string[]
}

type FieldCandidateRule = {
  headerPatterns: RegExp[]
  samplePatterns: RegExp[]
  minSampleMatches: number
  preferUniqueHeaderMatch?: boolean
}

const HOST_SAMPLE_PATTERNS = [
  /\b(?:25[0-5]|2[0-4]\d|1?\d?\d)(?:\.(?:25[0-5]|2[0-4]\d|1?\d?\d)){3}\b/,
  /\b[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)+\b/i,
  /^\s*[^:\s/]+:\d{1,5}\s*$/,
]

const IEC61850_ADDRESS_SAMPLE_PATTERNS = [
  /\b[A-Za-z0-9_-]+\/[A-Za-z0-9_.-]+(?:\.[A-Za-z0-9_.-]+)*(?:\[[A-Za-z0-9_.-]+\])?\b/,
  /\b[A-Za-z0-9_-]+\/[A-Za-z0-9_.-]+\$[A-Za-z0-9_.-]+\b/,
  /\b(?:LLN0|LPHD|XCBR|CSWI|PTOC|PDIS|GGIO|MMXU|SPC|DPC|DPS)\b/i,
]

const DEVICE_NAME_SAMPLE_PATTERNS = [
  /^[A-Za-z][A-Za-z0-9_.-]{1,31}$/,
  /^[A-Z][A-Z0-9_-]{1,31}$/,
]

const ACCESS_POINT_SAMPLE_PATTERNS = [
  /^[A-Za-z][A-Za-z0-9_.-]{0,15}$/,
  /^P\d+$/i,
  /^AP\d+$/i,
]

const PORT_SAMPLE_PATTERNS = [
  /^\d{2,5}$/,
]

const LOGICAL_DEVICE_SAMPLE_PATTERNS = [
  /^[A-Za-z][A-Za-z0-9_-]{0,15}$/,
]

const LOGICAL_NODE_SAMPLE_PATTERNS = [
  /^[A-Z0-9_]{2,16}$/,
]

const DATA_SET_SAMPLE_PATTERNS = [
  /\b[A-Za-z0-9_-]+\/[A-Za-z0-9_.-]+(?:\.[A-Za-z0-9_.-]+)+\b/,
  /\bDataSet\b/i,
]

const REPORT_CONTROL_SAMPLE_PATTERNS = [
  /\b(?:brcb|urcb|rcb|report control|reportcontrol|rptid|rpt id)\b/i,
  /\b[A-Za-z0-9_-]+\/[A-Za-z0-9_.-]+(?:\.brcb|\.(?:br|ur)cb)?\b/i,
]

export function detectVerificationImportHints(columns: readonly VerificationImportColumn[], rows: readonly unknown[][]): VerificationImportHints {
  const fieldHints = {
    transport_reference_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\bendpoint\b/i, /\btransport\b/i, /\bhost\b/i, /\bip\b/i, /\baddress\b/i, /\bmms\b/i],
      samplePatterns: HOST_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    transport_host_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\bhost\b/i, /\bip\b/i, /\baddress\b/i, /\bmms\b/i],
      samplePatterns: HOST_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    transport_port_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\bport\b/i, /\btcp\b/i, /\bmms\b/i],
      samplePatterns: PORT_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    ied_name_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\bied\b/i, /\bdevice\b/i, /\bunit\b/i, /\bfeeder\b/i],
      samplePatterns: DEVICE_NAME_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    access_point_name_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\baccess\s*point\b/i, /\baccess_point\b/i, /\bap\b/i],
      samplePatterns: ACCESS_POINT_SAMPLE_PATTERNS,
      minSampleMatches: 1,
      preferUniqueHeaderMatch: true,
    }),
    iec61850_address_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\b61850\b/i, /\biec\s*61850\b/i, /\bmodel\b/i, /\baddress\b/i, /\breference\b/i, /\bpath\b/i],
      samplePatterns: IEC61850_ADDRESS_SAMPLE_PATTERNS,
      minSampleMatches: 1,
      preferUniqueHeaderMatch: true,
    }),
    logical_device_inst_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\blogical\s*device\b/i, /\bld\b/i, /\bld\s*inst\b/i, /\blogical_device\b/i],
      samplePatterns: LOGICAL_DEVICE_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    logical_node_name_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\blogical\s*node\b/i, /\bln\b/i, /\bln\s*name\b/i, /\blogical_node\b/i],
      samplePatterns: LOGICAL_NODE_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    data_set_reference_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\bdata\s*set\b/i, /\bdataset\b/i, /\bfcda\b/i],
      samplePatterns: DATA_SET_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
    report_control_reference_column_hint: detectFieldHint(columns, rows, {
      headerPatterns: [/\breport\s*control\b/i, /\brcb\b/i, /\bbrcb\b/i, /\burcb\b/i, /\brpt\b/i],
      samplePatterns: REPORT_CONTROL_SAMPLE_PATTERNS,
      minSampleMatches: 1,
    }),
  }

  const notes: string[] = []
  if (fieldHints.transport_reference_column_hint.column) {
    notes.push(`Transport reference candidate: ${fieldHints.transport_reference_column_hint.column}`)
  }
  if (fieldHints.iec61850_address_column_hint.column) {
    notes.push(`IEC 61850 address candidate: ${fieldHints.iec61850_address_column_hint.column}`)
  }
  if (!fieldHints.transport_reference_column_hint.column && !fieldHints.transport_host_column_hint.column) {
    notes.push("No strong transport column was detected.")
  }
  if (!fieldHints.iec61850_address_column_hint.column && !fieldHints.ied_name_column_hint.column && !fieldHints.access_point_name_column_hint.column) {
    notes.push("No strong IEC 61850 model column was detected.")
  }

  return {
    ...fieldHints,
    notes,
  }
}

export function collectVerificationHintColumns(hints: VerificationImportHints): string[] {
  const seen = new Set<string>()
  const columns: string[] = []
  const values = Object.values(hints).filter(value => value && typeof value === "object" && "column" in value) as VerificationImportFieldHint[]
  values.forEach((hint) => {
    const column = String(hint.column ?? "").trim()
    if (!column || seen.has(column)) {
      return
    }
    seen.add(column)
    columns.push(column)
  })
  return columns
}

function detectFieldHint(
  columns: readonly VerificationImportColumn[],
  rows: readonly unknown[][],
  rule: FieldCandidateRule,
): VerificationImportFieldHint {
  type BestMatch = {
    column: string
    confidence: VerificationImportHintConfidence
    reason: string
    score: number
    sampleValues: string[]
  }
  let best: BestMatch | null = null

  for (const column of columns) {
    const header = normalizeHeader(column.header)
    const sampleValues = sampleColumnValues(rows, column.index)
    const headerMatchCount = rule.headerPatterns.reduce((count, pattern) => (
      pattern.test(header) ? count + 1 : count
    ), 0)
    const sampleMatchCount = sampleValues.reduce((count, sample) => (
      rule.samplePatterns.some(pattern => pattern.test(sample)) ? count + 1 : count
    ), 0)

    if (headerMatchCount === 0 && sampleMatchCount < rule.minSampleMatches) {
      continue
    }

    let score = 0
    if (headerMatchCount > 0) {
      score += 3 + Math.min(2, headerMatchCount)
    }
    if (sampleMatchCount > 0) {
      score += Math.min(4, sampleMatchCount * 2)
    }
    if (rule.preferUniqueHeaderMatch && headerMatchCount > 0 && sampleMatchCount === 0) {
      score += 1
    }

    const confidence = resolveConfidence({ headerMatchCount, sampleMatchCount, rule })
    const reason = buildReason({ column: column.header, headerMatchCount, sampleMatchCount, confidence })

    if (!best || score > best.score || (score === best.score && confidenceRank(confidence) > confidenceRank(best.confidence))) {
      best = {
        column: column.header,
        confidence,
        reason,
        score,
        sampleValues,
      }
    }
  }

  if (!best) {
    return {
      column: null,
      confidence: "missing",
      reason: "No strong column match was detected.",
      sample_values: [],
    }
  }

  return {
    column: best.column,
    confidence: best.confidence,
    reason: best.reason,
    sample_values: best.sampleValues,
  }
}

function resolveConfidence(args: {
  headerMatchCount: number
  sampleMatchCount: number
  rule: FieldCandidateRule
}): VerificationImportHintConfidence {
  const { headerMatchCount, sampleMatchCount, rule } = args
  if (headerMatchCount > 0 && sampleMatchCount >= 2) {
    return "exact"
  }
  if (headerMatchCount > 0 || sampleMatchCount >= 2) {
    return "likely"
  }
  if (sampleMatchCount >= rule.minSampleMatches) {
    return "possible"
  }
  return "missing"
}

function confidenceRank(value: VerificationImportHintConfidence): number {
  switch (value) {
    case "exact":
      return 3
    case "likely":
      return 2
    case "possible":
      return 1
    default:
      return 0
  }
}

function buildReason(args: {
  column: string
  headerMatchCount: number
  sampleMatchCount: number
  confidence: VerificationImportHintConfidence
}): string {
  const parts: string[] = []
  if (args.headerMatchCount > 0) {
    parts.push("header matched")
  }
  if (args.sampleMatchCount > 0) {
    parts.push(`${args.sampleMatchCount} sample${args.sampleMatchCount === 1 ? "" : "s"} matched`)
  }
  if (!parts.length) {
    parts.push("weak indirect match")
  }
  parts.push(`confidence ${args.confidence}`)
  return `${args.column}: ${parts.join(", ")}`
}

function normalizeHeader(value: string): string {
  return String(value ?? "")
    .trim()
    .toLowerCase()
    .replace(/[_\-.\/]+/g, " ")
    .replace(/\s+/g, " ")
}

function sampleColumnValues(rows: readonly unknown[][], columnIndex: number, limit = 5): string[] {
  const values: string[] = []
  for (let rowIndex = 1; rowIndex < rows.length; rowIndex += 1) {
    const row = Array.isArray(rows[rowIndex]) ? rows[rowIndex] : []
    const raw = row[columnIndex]
    const value = normalizeCellValue(raw)
    if (!value) {
      continue
    }
    values.push(value)
    if (values.length >= limit) {
      break
    }
  }
  return values
}

function normalizeCellValue(value: unknown): string {
  if (value === null || typeof value === "undefined") {
    return ""
  }
  if (typeof value === "string") {
    return value.trim()
  }
  if (value instanceof Date) {
    return value.toISOString().trim()
  }
  return String(value).trim()
}
