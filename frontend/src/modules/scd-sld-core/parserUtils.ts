import type { ScdDiagnostic, ScdSourceLocation } from "./types"
import { readXmlAttribute, type XmlElementEvent } from "./xmlScanner"

export function readRequiredName(event: XmlElementEvent): string {
  return readXmlAttribute(event.attributes, "name")?.trim() || "unnamed"
}

export type DuplicateScopedIdTracker = {
  record(input: {
    entityKind: string
    baseId: string
    generatedId: string
    sourcePath: string
    sourceLocation?: ScdSourceLocation
  }): void
  flush(diagnostics: ScdDiagnostic[]): void
}

export function createDuplicateScopedIdTracker(): DuplicateScopedIdTracker {
  const groups = new Map<string, DuplicateScopedIdGroup>()

  return {
    record(input) {
      const key = `${input.entityKind}\u0000${input.baseId}`
      const existing = groups.get(key)
      if (existing) {
        existing.occurrences.push({
          generatedId: input.generatedId,
          sourcePath: input.sourcePath,
          sourceLocation: input.sourceLocation,
        })
        return
      }

      groups.set(key, {
        entityKind: input.entityKind,
        baseId: input.baseId,
        parentScope: deriveParentScope(input.baseId),
        name: deriveScopedName(input.baseId),
        occurrences: [{
          generatedId: input.generatedId,
          sourcePath: input.sourcePath,
          sourceLocation: input.sourceLocation,
        }],
      })
    },
    flush(diagnostics) {
      for (const group of groups.values()) {
        if (group.occurrences.length <= 1) {
          continue
        }

        const firstOccurrence = group.occurrences[0]
        if (!firstOccurrence) {
          continue
        }

        const generatedIds = group.occurrences.map(occurrence => occurrence.generatedId)
        diagnostics.push({
          severity: "warning",
          stage: "normalizer",
          code: "normalizer.duplicate-normalized-id",
          message: `${group.entityKind} "${group.name}" under ${group.parentScope} appears ${group.occurrences.length} times; generated stable ids ${formatGeneratedIds(generatedIds)}.`,
          sourcePath: firstOccurrence.sourcePath,
          sourceId: generatedIds[generatedIds.length - 1] ?? group.baseId,
          sourceLocation: firstOccurrence.sourceLocation,
          context: {
            entityKind: group.entityKind,
            parentScope: group.parentScope,
            normalizedId: group.baseId,
            duplicateCount: group.occurrences.length,
            generatedIds,
          },
        })
      }
    },
  }
}

export function makeUniqueScopedId(input: {
  baseId: string
  existingIds: string[]
  diagnostics: ScdDiagnostic[]
  event: XmlElementEvent
  entityKind: string
  duplicateTracker?: DuplicateScopedIdTracker
}): string {
  const existingIds = new Set(input.existingIds)
  if (!existingIds.has(input.baseId)) {
    if (input.duplicateTracker) {
      input.duplicateTracker.record({
        entityKind: input.entityKind,
        baseId: input.baseId,
        generatedId: input.baseId,
        sourcePath: input.event.sourcePath,
        sourceLocation: input.event.sourceLocation,
      })
    }
    return input.baseId
  }

  let suffix = 2
  let id = `${input.baseId}__${suffix}`
  while (existingIds.has(id)) {
    suffix += 1
    id = `${input.baseId}__${suffix}`
  }

  if (input.duplicateTracker) {
    input.duplicateTracker.record({
      entityKind: input.entityKind,
      baseId: input.baseId,
      generatedId: id,
      sourcePath: input.event.sourcePath,
      sourceLocation: input.event.sourceLocation,
    })
    return id
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
export function flushDuplicateIdDiagnostics(tracker: DuplicateScopedIdTracker, diagnostics: ScdDiagnostic[]) {
  tracker.flush(diagnostics)
}

type DuplicateScopedIdGroup = {
  entityKind: string
  baseId: string
  parentScope: string
  name: string
  occurrences: Array<{
    generatedId: string
    sourcePath: string
    sourceLocation?: ScdSourceLocation
  }>
}

function deriveScopedName(baseId: string): string {
  const parts = baseId.split("/").filter(Boolean)
  return parts[parts.length - 1] ?? baseId
}

function deriveParentScope(baseId: string): string {
  const parts = baseId.split("/").filter(Boolean)
  if (parts.length <= 2) {
    return parts.slice(0, -1).join("/") || baseId
  }
  return parts.slice(0, -2).join("/") || baseId
}

function formatGeneratedIds(ids: string[]): string {
  const labels = ids.map(deriveScopedName)
  if (labels.length <= 4) {
    return labels.join(", ")
  }

  return `${labels[0]}, ${labels[1]}, ..., ${labels[labels.length - 1]}`
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
