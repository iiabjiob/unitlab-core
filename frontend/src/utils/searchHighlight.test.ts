import { describe, expect, it } from "vitest"
import { splitSearchHighlightText } from "./searchHighlight"

describe("splitSearchHighlightText", () => {
  it("highlights case-insensitive matches", () => {
    expect(splitSearchHighlightText("Transformer Report Alpha", "report")).toEqual([
      { text: "Transformer ", matched: false },
      { text: "Report", matched: true },
      { text: " Alpha", matched: false },
    ])
  })

  it("preserves the original text when query is empty", () => {
    expect(splitSearchHighlightText("Alpha", " ")).toEqual([
      { text: "Alpha", matched: false },
    ])
  })

  it("highlights multiple occurrences", () => {
    expect(splitSearchHighlightText("Signal signal SIGNAL", "signal")).toEqual([
      { text: "Signal", matched: true },
      { text: " ", matched: false },
      { text: "signal", matched: true },
      { text: " ", matched: false },
      { text: "SIGNAL", matched: true },
    ])
  })
})
