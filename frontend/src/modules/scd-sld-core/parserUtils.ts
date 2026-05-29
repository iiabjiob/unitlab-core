import type { ScdDiagnostic } from "./types"
import { readXmlAttribute, type XmlElementEvent } from "./xmlScanner"

export function readRequiredName(event: XmlElementEvent): string {
  return readXmlAttribute(event.attributes, "name")?.trim() || "unnamed"
}

export function makeUniqueScopedId(input: {
  baseId: string
  existingIds: string[]
  diagnostics: ScdDiagnostic[]
  event: XmlElementEvent
  entityKind: string
}): string {
  const existingIds = new Set(input.existingIds)
  if (!existingIds.has(input.baseId)) {
    return input.baseId
  }

  let suffix = 2
  let id = `${input.baseId}__${suffix}`
  while (existingIds.has(id)) {
    suffix += 1
    id = `${input.baseId}__${suffix}`
  }

  input.diagnostics.push({
    severity: "warning",
    stage: "normalizer",
    code: "normalizer.duplicate-normalized-id",
    message: `${input.entityKind} normalized id "${input.baseId}" is duplicated in the same parent scope; it was disambiguated as "${id}".`,
    sourcePath: input.event.sourcePath,
    sourceId: id,
    sourceLocation: input.event.sourceLocation,
  })

  return id
}

export function buildChildId(parentId: string, childKind: string, childName: string): string {
  return `${parentId}/${buildStableId([childKind, childName])}`
}

export function buildStableId(parts: string[]): string {
  return parts
    .map(part => part.trim())
    .filter(Boolean)
    .map(part => part.replace(/[\s/]+/g, "_"))
    .join("/")
}

export function parseBooleanAttribute(value: string | null): boolean | null {
  const normalized = value?.trim().toLowerCase()
  if (normalized === "true" || normalized === "1") {
    return true
  }
  if (normalized === "false" || normalized === "0") {
    return false
  }
  return null
}

export function parseNullableInteger(value: string | null): number | null {
  if (value === null || value.trim() === "") {
    return null
  }
  const numeric = Number.parseInt(value, 10)
  return Number.isFinite(numeric) ? numeric : null
}

export function parseNullableNumber(value: string | null): number | null {
  if (value === null || value.trim() === "") {
    return null
  }
  const numeric = Number(value)
  return Number.isFinite(numeric) ? numeric : null
}

export function pushParentDiagnostic(
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
  child: string,
  parent: string,
) {
  diagnostics.push({
    severity: "warning",
    stage: "parser",
    code: "parser.missing-parent",
    message: `${child} is outside ${parent}; it was skipped.`,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  })
}

export function normalizeConnectivityNodePath(input: {
  pathName: string | null
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  nodeName: string | null
  fallbackPath: string
}): string {
  if (input.pathName?.trim()) {
    return normalizePath(input.pathName)
  }

  return normalizePathParts([
    input.substationName,
    input.voltageLevelName,
    input.bayName,
    input.nodeName,
  ]) ?? input.fallbackPath
}

export function normalizePath(value: string): string {
  return value.split("/").map(part => part.trim()).filter(Boolean).join("/")
}

export function normalizePathParts(parts: Array<string | null>): string | null {
  const path = parts.map(part => part?.trim() ?? "").filter(Boolean).join("/")
  return path || null
}

export function uniqueStrings(items: string[]): string[] {
  return Array.from(new Set(items))
}

export function isPresent<T>(value: T | null | undefined): value is T {
  return value !== null && value !== undefined
}

export function mergeUniqueDiagnostics(target: ScdDiagnostic[], source: ScdDiagnostic[]) {
  const seen = new Set(target.map(diagnosticFingerprint))
  for (const diagnostic of source) {
    const key = diagnosticFingerprint(diagnostic)
    if (seen.has(key)) {
      continue
    }
    seen.add(key)
    target.push(diagnostic)
  }
}

function diagnosticFingerprint(diagnostic: ScdDiagnostic): string {
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

export function last<T>(items: T[]): T | null {
  return items[items.length - 1] ?? null
}
