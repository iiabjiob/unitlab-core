export type SearchHighlightSegment = {
  text: string
  matched: boolean
}

export function splitSearchHighlightText(text: string, query: string): SearchHighlightSegment[] {
  const normalizedQuery = query.trim().toLowerCase()
  if (!text || !normalizedQuery) {
    return [{ text, matched: false }]
  }

  const normalizedText = text.toLowerCase()
  const segments: SearchHighlightSegment[] = []
  let cursor = 0

  while (cursor < text.length) {
    const matchIndex = normalizedText.indexOf(normalizedQuery, cursor)
    if (matchIndex === -1) {
      const remainder = text.slice(cursor)
      if (remainder) {
        segments.push({ text: remainder, matched: false })
      }
      break
    }

    if (matchIndex > cursor) {
      segments.push({ text: text.slice(cursor, matchIndex), matched: false })
    }

    const matchEnd = matchIndex + normalizedQuery.length
    segments.push({ text: text.slice(matchIndex, matchEnd), matched: true })
    cursor = matchEnd
  }

  return segments.length ? segments : [{ text, matched: false }]
}
