import { describe, expect, it } from "vitest"
import {
  advanceCoreDiagnosticsIncidentDebounce,
  buildCoreDiagnosticsIssueSignature,
} from "./coreDiagnosticsIncident"

describe("core diagnostics incident signature", () => {
  it("ignores changing numeric measurements for the same incident", () => {
    expect(buildCoreDiagnosticsIssueSignature("degraded", ["CPU temp 86.0°C", "memory 96.0%"]))
      .toBe(buildCoreDiagnosticsIssueSignature("degraded", ["CPU temp 91.5°C", "memory 97.2%"]))
  })

  it("keeps different inactive required services as separate incidents", () => {
    expect(buildCoreDiagnosticsIssueSignature("error", ["services inactive (docker)"]))
      .not.toBe(buildCoreDiagnosticsIssueSignature("error", ["services inactive (NetworkManager)"]))
  })

  it("promotes an incident only after two consecutive snapshots", () => {
    const signature = buildCoreDiagnosticsIssueSignature("degraded", ["memory 96.0%"])
    const first = advanceCoreDiagnosticsIncidentDebounce(null, 0, signature)
    const second = advanceCoreDiagnosticsIncidentDebounce(first.signature, first.count, signature)

    expect(first.stable).toBe(false)
    expect(second.stable).toBe(true)
    expect(second.count).toBe(2)
  })
})
