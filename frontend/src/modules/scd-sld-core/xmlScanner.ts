import type { ScdDiagnostic, ScdSourceLocation } from "./types"

export type XmlAttributes = Record<string, string>

export type XmlElementEvent = {
  kind: "open" | "close"
  name: string
  localName: string
  attributes: XmlAttributes
  selfClosing: boolean
  textContent: string | null
  sourcePath: string
  sourceLocation: ScdSourceLocation
  depth: number
}

export type XmlElementRange = {
  name: string
  localName: string
  text: string
  startOffset: number
  sourceLocation: ScdSourceLocation
}

export type XmlScannerOptions = {
  baseOffset?: number
  baseLine?: number
  baseColumn?: number
  sourcePathPrefix?: readonly string[]
}

type StackFrame = {
  localName: string
  segment: string
  childCounts: Record<string, number>
  sourceLocation: ScdSourceLocation
}

const XML_TAG_PATTERN = /<[^>]+>/g
const XML_ATTRIBUTE_PATTERN = /([^\s"'=<>`]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/g

// Lightweight SCL scanner boundary, not a generic XML parser. It is designed
// for standard SCD element/attribute traversal in this core slice and does not
// attempt DTD/CDATA processing, namespace URI resolution, or full XML recovery.
export function* scanXmlElements(
  xmlText: string,
  diagnostics: ScdDiagnostic[] = [],
  options: XmlScannerOptions = {},
): Generator<XmlElementEvent> {
  const stack: StackFrame[] = []
  const locationTracker = createLineTracker(xmlText, options)
  const sourcePathPrefix = options.sourcePathPrefix ?? []
  let match: RegExpExecArray | null
  XML_TAG_PATTERN.lastIndex = 0

  try {
    while ((match = XML_TAG_PATTERN.exec(xmlText)) !== null) {
      const rawTag = match[0]
      const parsed = parseRawTag(rawTag)
      if (!parsed) {
        continue
      }
      locationTracker.advanceTo(match.index)
      const sourceLocation = locationTracker.locationAt(match.index)

      if (parsed.kind === "close") {
        const frame = stack[stack.length - 1]
        const sourcePath = [...sourcePathPrefix, ...stack.map(item => item.segment)].join("/")
        if (!frame || frame.localName !== parsed.localName) {
          diagnostics.push({
            severity: "warning",
            stage: "xml",
            code: "xml.mismatched-close-tag",
            message: `Unexpected closing tag </${parsed.name}>.`,
            sourcePath: sourcePath || parsed.localName,
            sourceLocation,
          })
        } else {
          stack.pop()
        }

        yield {
          kind: "close",
          name: parsed.name,
          localName: parsed.localName,
          attributes: {},
          selfClosing: false,
          textContent: null,
          sourcePath: sourcePath || parsed.localName,
          sourceLocation,
          depth: sourcePathPrefix.length + stack.length,
        }
        continue
      }

      const parent = stack[stack.length - 1] ?? null
      const occurrence = nextChildOccurrence(parent, parsed.localName)
      const segment = buildSourcePathSegment(parsed.localName, parsed.attributes, occurrence)
      const sourcePath = [...sourcePathPrefix, ...stack.map(item => item.segment), segment].join("/")

      yield {
        kind: "open",
        name: parsed.name,
        localName: parsed.localName,
        attributes: parsed.attributes,
        selfClosing: parsed.selfClosing,
        textContent: parsed.selfClosing ? null : readImmediateTextContent(xmlText, XML_TAG_PATTERN.lastIndex),
        sourcePath,
        sourceLocation,
        depth: sourcePathPrefix.length + stack.length,
      }

      if (!parsed.selfClosing) {
        stack.push({
          localName: parsed.localName,
          segment,
          childCounts: {},
          sourceLocation,
        })
      }
    }

    for (let index = stack.length - 1; index >= 0; index -= 1) {
      const frame = stack[index]
      if (!frame) {
        continue
      }
      diagnostics.push({
        severity: "warning",
        stage: "xml",
        code: "xml.unclosed-tag",
        message: `Unclosed tag <${frame.localName}>.`,
        sourcePath: [...sourcePathPrefix, ...stack.slice(0, index + 1).map(item => item.segment)].join("/"),
        sourceLocation: frame.sourceLocation,
      })
    }
  } finally {
    XML_TAG_PATTERN.lastIndex = 0
  }
}

export function findXmlElementRanges(
  xmlText: string,
  localNames: readonly string[],
  diagnostics: ScdDiagnostic[] = [],
): XmlElementRange[] {
  const targetNames = new Set(localNames)
  const ranges: XmlElementRange[] = []
  const openTargets: Array<{
    name: string
    localName: string
    startOffset: number
    sourceLocation: ScdSourceLocation
  }> = []
  const locationTracker = createLineTracker(xmlText)
  let match: RegExpExecArray | null
  XML_TAG_PATTERN.lastIndex = 0

  try {
    while ((match = XML_TAG_PATTERN.exec(xmlText)) !== null) {
      const parsed = parseRawTagName(match[0])
      if (!parsed) {
        continue
      }
      locationTracker.advanceTo(match.index)
      const sourceLocation = locationTracker.locationAt(match.index)

      if (parsed.kind === "open") {
        if (!targetNames.has(parsed.localName)) {
          continue
        }

        if (parsed.selfClosing) {
          ranges.push({
            name: parsed.name,
            localName: parsed.localName,
            text: match[0],
            startOffset: match.index,
            sourceLocation,
          })
          continue
        }

        openTargets.push({
          name: parsed.name,
          localName: parsed.localName,
          startOffset: match.index,
          sourceLocation,
        })
        continue
      }

      const active = openTargets[openTargets.length - 1]
      if (!active || active.name !== parsed.name) {
        continue
      }

      openTargets.pop()
      ranges.push({
        name: active.name,
        localName: active.localName,
        text: xmlText.slice(active.startOffset, XML_TAG_PATTERN.lastIndex),
        startOffset: active.startOffset,
        sourceLocation: active.sourceLocation,
      })
    }

    for (const active of openTargets) {
      diagnostics.push({
        severity: "warning",
        stage: "xml",
        code: "xml.unclosed-tag",
        message: `Unclosed tag <${active.name}>.`,
        sourcePath: active.localName,
        sourceLocation: active.sourceLocation,
      })
    }
  } finally {
    XML_TAG_PATTERN.lastIndex = 0
  }

  return ranges
}

function createLineTracker(xmlText: string, options: XmlScannerOptions = {}) {
  const baseOffset = options.baseOffset ?? 0
  let cursor = 0
  let line = options.baseLine ?? 1
  let lineStartOffset = 1 - (options.baseColumn ?? 1)

  return {
    advanceTo(offset: number) {
      while (cursor < offset) {
        const newlineIndex = xmlText.indexOf("\n", cursor)
        if (newlineIndex === -1 || newlineIndex >= offset) {
          cursor = offset
          return
        }
        line += 1
        lineStartOffset = newlineIndex + 1
        cursor = newlineIndex + 1
      }
    },
    locationAt(offset: number): ScdSourceLocation {
      return {
        line,
        column: offset - lineStartOffset + 1,
        offset: baseOffset + offset,
      }
    },
  }
}

function parseRawTag(rawTag: string):
  | { kind: "open"; name: string; localName: string; attributes: XmlAttributes; selfClosing: boolean }
  | { kind: "close"; name: string; localName: string }
  | null {
  const inner = rawTag.slice(1, -1).trim()
  if (!inner || inner.startsWith("?") || inner.startsWith("!")) {
    return null
  }

  if (inner.startsWith("/")) {
    const name = inner.slice(1).trim().split(/\s+/)[0] ?? ""
    if (!name) {
      return null
    }
    return {
      kind: "close",
      name,
      localName: getLocalName(name),
    }
  }

  const selfClosing = inner.endsWith("/")
  const normalized = selfClosing ? inner.slice(0, -1).trim() : inner
  const name = normalized.split(/\s+/)[0] ?? ""
  if (!name) {
    return null
  }

  const attributes: XmlAttributes = {}
  for (const attrMatch of normalized.matchAll(XML_ATTRIBUTE_PATTERN)) {
    const key = attrMatch[1]
    if (!key) {
      continue
    }
    attributes[key] = decodeXmlEntities(attrMatch[2] ?? attrMatch[3] ?? "")
  }

  return {
    kind: "open",
    name,
    localName: getLocalName(name),
    attributes,
    selfClosing,
  }
}

function parseRawTagName(rawTag: string):
  | { kind: "open"; name: string; localName: string; selfClosing: boolean }
  | { kind: "close"; name: string; localName: string }
  | null {
  const inner = rawTag.slice(1, -1).trim()
  if (!inner || inner.startsWith("?") || inner.startsWith("!")) {
    return null
  }

  if (inner.startsWith("/")) {
    const name = inner.slice(1).trim().split(/\s+/)[0] ?? ""
    if (!name) {
      return null
    }
    return {
      kind: "close",
      name,
      localName: getLocalName(name),
    }
  }

  const selfClosing = inner.endsWith("/")
  const normalized = selfClosing ? inner.slice(0, -1).trim() : inner
  const name = normalized.split(/\s+/)[0] ?? ""
  if (!name) {
    return null
  }

  return {
    kind: "open",
    name,
    localName: getLocalName(name),
    selfClosing,
  }
}

export function getLocalName(qualifiedName: string): string {
  const parts = qualifiedName.split(":")
  return parts[parts.length - 1] ?? qualifiedName
}

export function readXmlAttribute(attributes: XmlAttributes, name: string): string | null {
  return attributes[name] ?? null
}

export function readXmlAttributeByLocalName(attributes: XmlAttributes, localName: string): string | null {
  for (const [key, value] of Object.entries(attributes)) {
    if (getLocalName(key) === localName) {
      return value
    }
  }
  return null
}

function nextChildOccurrence(parent: StackFrame | null, localName: string): number {
  if (!parent) {
    return 1
  }
  const next = (parent.childCounts[localName] ?? 0) + 1
  parent.childCounts[localName] = next
  return next
}

function buildSourcePathSegment(localName: string, attributes: XmlAttributes, occurrence: number): string {
  const named = readXmlAttribute(attributes, "name") ?? readXmlAttribute(attributes, "id")
  const suffix = named ? sanitizePathPart(named) : `#${occurrence}`
  return `${localName}:${suffix}`
}

function sanitizePathPart(value: string): string {
  const normalized = value.trim()
  return normalized ? normalized.replace(/[\s/]+/g, "_") : "unnamed"
}

function decodeXmlEntities(value: string): string {
  return value
    .replace(/&quot;/g, "\"")
    .replace(/&apos;/g, "'")
    .replace(/&lt;/g, "<")
    .replace(/&gt;/g, ">")
    .replace(/&amp;/g, "&")
}

function readImmediateTextContent(xmlText: string, offset: number): string | null {
  const nextTagIndex = xmlText.indexOf("<", offset)
  const rawText = xmlText.slice(offset, nextTagIndex === -1 ? xmlText.length : nextTagIndex)
  const text = decodeXmlEntities(rawText).trim()
  return text || null
}
