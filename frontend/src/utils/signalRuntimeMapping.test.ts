import { describe, expect, it } from "vitest"

import { resolveRuntimeChannelTypeForSignal } from "./signalRuntimeMapping"

describe("resolveRuntimeChannelTypeForSignal", () => {
  it("maps project directions to inverted simulator channel types", () => {
    expect(resolveRuntimeChannelTypeForSignal("DI")).toBe("do")
    expect(resolveRuntimeChannelTypeForSignal("DO")).toBe("di")
    expect(resolveRuntimeChannelTypeForSignal("AI")).toBe("ao")
    expect(resolveRuntimeChannelTypeForSignal("AO")).toBe("ai")
  })

  it("normalizes whitespace and casing", () => {
    expect(resolveRuntimeChannelTypeForSignal("  di ")).toBe("do")
    expect(resolveRuntimeChannelTypeForSignal(" do")).toBe("di")
    expect(resolveRuntimeChannelTypeForSignal("Ai ")).toBe("ao")
    expect(resolveRuntimeChannelTypeForSignal(" aO ")).toBe("ai")
  })

  it("returns null for unknown or empty values", () => {
    expect(resolveRuntimeChannelTypeForSignal("")).toBeNull()
    expect(resolveRuntimeChannelTypeForSignal("xx")).toBeNull()
    expect(resolveRuntimeChannelTypeForSignal("DI_DO")).toBeNull()
  })
})
