import { describe, expect, it } from "vitest"

import { normalizeIec61850Reference } from "./iec61850Reference"

describe("normalizeIec61850Reference", () => {
  it("normalizes slash and MMS-style references to the same canonical form", () => {
    const left = normalizeIec61850Reference("KINTE15BCU01CTRL1/CBCSWI1/Pos/Oper.ctlVal[CO]")
    const right = normalizeIec61850Reference("KINTE15BCU01CTRL1/CBCSWI1.Pos.Oper.ctlVal[CO]")
    const mms = normalizeIec61850Reference("KINTE15BCU01CTRL1/CBCSWI1$CO$Pos$Oper.ctlVal")

    expect(left).toBe(right)
    expect(mms).toBe(right)
  })
})
