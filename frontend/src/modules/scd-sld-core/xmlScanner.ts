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

const XML_ATTRIBUTE_PATTERN = /([^\s"'=<>`]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/g
const XML_DECLARATION_PREFIX = "<?"
const XML_COMMENT_PREFIX = "<!--"
const XML_CDATA_PREFIX = "<![CDATA["
const XML_DOCTYPE_PREFIX = "<!DOCTYPE"

// Lightweight SCL scanner boundary, not a generic XML parser. It is designed
// for standard SCD element/attribute traversal in this core slice and skips
// comments, CDATA and processing instructions without attempting full XML
// recovery.
export function* scanXmlElements(
  xmlText: string,
  diagnostics: ScdDiagnostic[] = [],
  options: XmlScannerOptions = {},
): Generator<XmlElementEvent> {
  const stack: StackFrame[] = []
  const locationTracker = createLineTracker(xmlText, options)
  const sourcePathPrefix = options.sourcePathPrefix ?? []
  let cursor = 0

  while (cursor < xmlText.length) {
    const tagStart = xmlText.indexOf("<", cursor)
    if (tagStart === -1) {
      break
    }

    const tag = readXmlTag(xmlText, tagStart)
    if (!tag) {
      cursor = tagStart + 1
      continue
    }

    locationTracker.advanceTo(tagStart)
    const sourceLocation = locationTracker.locationAt(tagStart)

    if (tag.kind === "skip") {
      cursor = tag.endOffset
      continue
    }

    const parsed = parseRawTag(tag.rawTag)
    if (!parsed) {
      cursor = tag.endOffset
      continue
    }

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
      cursor = tag.endOffset
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
      textContent: parsed.selfClosing ? null : readImmediateTextContent(xmlText, tag.endOffset),
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

    cursor = tag.endOffset
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
  let cursor = 0

  while (cursor < xmlText.length) {
    const tagStart = xmlText.indexOf("<", cursor)
    if (tagStart === -1) {
      break
    }

    const tag = readXmlTag(xmlText, tagStart)
    if (!tag) {
      cursor = tagStart + 1
      continue
    }

    locationTracker.advanceTo(tagStart)
    const sourceLocation = locationTracker.locationAt(tagStart)

    if (tag.kind === "skip") {
      cursor = tag.endOffset
      continue
    }

    const parsed = parseRawTagName(tag.rawTag)
    if (!parsed) {
      cursor = tag.endOffset
      continue
    }

    if (parsed.kind === "open") {
      if (!targetNames.has(parsed.localName)) {
        cursor = tag.endOffset
        continue
      }

      if (parsed.selfClosing) {
        ranges.push({
          name: parsed.name,
          localName: parsed.localName,
          text: tag.rawTag,
          startOffset: tagStart,
          sourceLocation,
        })
        cursor = tag.endOffset
        continue
      }

      openTargets.push({
        name: parsed.name,
        localName: parsed.localName,
        startOffset: tagStart,
        sourceLocation,
      })
      cursor = tag.endOffset
      continue
    }

    const active = openTargets[openTargets.length - 1]
    if (!active || active.name !== parsed.name) {
      cursor = tag.endOffset
      continue
    }

    openTargets.pop()
    ranges.push({
      name: active.name,
      localName: active.localName,
      text: xmlText.slice(active.startOffset, tag.endOffset),
      startOffset: active.startOffset,
      sourceLocation: active.sourceLocation,
    })
    cursor = tag.endOffset
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

  return ranges
}

type XmlTag =
  | { kind: "open"; rawTag: string; endOffset: number }
  | { kind: "close"; rawTag: string; endOffset: number }
  | { kind: "skip"; endOffset: number }

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

function readXmlTag(xmlText: string, startOffset: number): XmlTag | null {
  if (xmlText[startOffset] !== "<") {
    return null
  }

  if (xmlText.startsWith(XML_COMMENT_PREFIX, startOffset)) {
    const endOffset = xmlText.indexOf("-->", startOffset + XML_COMMENT_PREFIX.length)
    return { kind: "skip", endOffset: endOffset === -1 ? xmlText.length : endOffset + 3 }
  }

  if (xmlText.startsWith(XML_CDATA_PREFIX, startOffset)) {
    const endOffset = xmlText.indexOf("]]>", startOffset + XML_CDATA_PREFIX.length)
    return { kind: "skip", endOffset: endOffset === -1 ? xmlText.length : endOffset + 3 }
  }

  if (xmlText.startsWith(XML_DECLARATION_PREFIX, startOffset)) {
    const endOffset = xmlText.indexOf("?>", startOffset + XML_DECLARATION_PREFIX.length)
    return { kind: "skip", endOffset: endOffset === -1 ? xmlText.length : endOffset + 2 }
  }

  if (xmlText.startsWith(XML_DOCTYPE_PREFIX, startOffset) || xmlText.startsWith("<!doctype", startOffset)) {
    const endOffset = findTagEnd(xmlText, startOffset)
    return { kind: "skip", endOffset: endOffset === -1 ? xmlText.length : endOffset + 1 }
  }

  const endOffset = findTagEnd(xmlText, startOffset)
  if (endOffset === -1) {
    return null
  }

  const rawTag = xmlText.slice(startOffset, endOffset + 1)
  return {
    kind: rawTag.startsWith("</") ? "close" : "open",
    rawTag,
    endOffset: endOffset + 1,
  }
}

function findTagEnd(xmlText: string, startOffset: number): number {
  let inSingleQuote = false
  let inDoubleQuote = false

  for (let index = startOffset + 1; index < xmlText.length; index += 1) {
    const char = xmlText[index]
    if (char === "'" && !inDoubleQuote) {
      inSingleQuote = !inSingleQuote
      continue
    }
    if (char === '"' && !inSingleQuote) {
      inDoubleQuote = !inDoubleQuote
      continue
    }
    if (char === ">" && !inSingleQuote && !inDoubleQuote) {
      return index
    }
  }

  return -1
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
    .replace(/&quot;/g, '"')
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
