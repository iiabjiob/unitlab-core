import type { ScdDiagnostic } from "./types"

export type XmlAttributes = Record<string, string>

export type XmlElementEvent = {
  kind: "open" | "close"
  name: string
  localName: string
  attributes: XmlAttributes
  selfClosing: boolean
  textContent: string | null
  sourcePath: string
  depth: number
}

type StackFrame = {
  localName: string
  segment: string
  childCounts: Record<string, number>
}

const XML_TAG_PATTERN = /<[^>]+>/g
const XML_ATTRIBUTE_PATTERN = /([^\s"'=<>`]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/g

export function* scanXmlElements(
  xmlText: string,
  diagnostics: ScdDiagnostic[] = [],
): Generator<XmlElementEvent> {
  const stack: StackFrame[] = []
  let match: RegExpExecArray | null
  XML_TAG_PATTERN.lastIndex = 0

  try {
    while ((match = XML_TAG_PATTERN.exec(xmlText)) !== null) {
      const rawTag = match[0]
      const parsed = parseRawTag(rawTag)
      if (!parsed) {
        continue
      }

      if (parsed.kind === "close") {
        const frame = stack[stack.length - 1]
        const sourcePath = stack.map(item => item.segment).join("/")
        if (!frame || frame.localName !== parsed.localName) {
          diagnostics.push({
            severity: "warning",
            stage: "xml",
            code: "xml.mismatched-close-tag",
            message: `Unexpected closing tag </${parsed.name}>.`,
            sourcePath: sourcePath || parsed.localName,
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
          depth: stack.length,
        }
        continue
      }

      const parent = stack[stack.length - 1] ?? null
      const occurrence = nextChildOccurrence(parent, parsed.localName)
      const segment = buildSourcePathSegment(parsed.localName, parsed.attributes, occurrence)
      const sourcePath = [...stack.map(item => item.segment), segment].join("/")

      yield {
        kind: "open",
        name: parsed.name,
        localName: parsed.localName,
        attributes: parsed.attributes,
        selfClosing: parsed.selfClosing,
        textContent: parsed.selfClosing ? null : readImmediateTextContent(xmlText, XML_TAG_PATTERN.lastIndex),
        sourcePath,
        depth: stack.length,
      }

      if (!parsed.selfClosing) {
        stack.push({
          localName: parsed.localName,
          segment,
          childCounts: {},
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
        sourcePath: stack.slice(0, index + 1).map(item => item.segment).join("/"),
      })
    }
  } finally {
    XML_TAG_PATTERN.lastIndex = 0
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
