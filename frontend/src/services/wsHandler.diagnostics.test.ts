import { describe, expect, it } from "vitest"
import { buildCoreDiagnosticsIssueSignature } from "./coreDiagnosticsIncident"

describe("core diagnostics incident signature", () => {
  it("ignores changing numeric measurements for the same incident", () => {
    expect(buildCoreDiagnosticsIssueSignature("degraded", ["CPU temp 86.0°C", "memory 96.0%"]))
      .toBe(buildCoreDiagnosticsIssueSignature("degraded", ["CPU temp 91.5°C", "memory 97.2%"]))
  })

  it("keeps different inactive required services as separate incidents", () => {
    expect(buildCoreDiagnosticsIssueSignature("error", ["services inactive (docker)"]))
      .not.toBe(buildCoreDiagnosticsIssueSignature("error", ["services inactive (NetworkManager)"]))
  })
})
