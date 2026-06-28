import { describe, expect, it } from "vitest"

import {
  collectVerificationHintColumns,
  detectVerificationImportHints,
} from "./verificationImportHints"

describe("verificationImportHints", () => {
  it("detects transport and IEC 61850 hint columns from headers and samples", () => {
    const columns = [
      { header: "Signal Name", index: 0 },
      { header: "MMS Endpoint", index: 1 },
      { header: "Port", index: 2 },
      { header: "IED Name", index: 3 },
      { header: "Access Point", index: 4 },
      { header: "61850 Address", index: 5 },
    ]
    const rows = [
      ["Signal Name", "MMS Endpoint", "Port", "IED Name", "Access Point", "61850 Address"],
      ["Breaker Close", "10.10.10.12", 102, "IED-A", "P1", "IED-A/P1/LLN0.brA"],
      ["Breaker Open", "10.10.10.12:102", 102, "IED-A", "P1", "IED-A/P1/LLN0.brA"],
    ]

    const hints = detectVerificationImportHints(columns, rows)

    expect(hints.transport_reference_column_hint.column).toBe("MMS Endpoint")
    expect(hints.transport_reference_column_hint.confidence).toBe("exact")
    expect(hints.transport_port_column_hint.column).toBe("Port")
    expect(hints.ied_name_column_hint.column).toBe("IED Name")
    expect(hints.access_point_name_column_hint.column).toBe("Access Point")
    expect(hints.iec61850_address_column_hint.column).toBe("61850 Address")
    expect(hints.iec61850_address_column_hint.confidence).toBe("exact")
    expect(collectVerificationHintColumns(hints)).toEqual([
      "MMS Endpoint",
      "Port",
      "IED Name",
      "Access Point",
      "61850 Address",
    ])
  })

  it("keeps verification hints empty when the workbook does not expose transport metadata", () => {
    const columns = [
      { header: "Signal Name", index: 0 },
      { header: "Category", index: 1 },
    ]
    const rows = [
      ["Signal Name", "Category"],
      ["Breaker Close", "DO"],
      ["Breaker Open", "DI"],
    ]

    const hints = detectVerificationImportHints(columns, rows)

    expect(hints.transport_reference_column_hint.column).toBeNull()
    expect(hints.iec61850_address_column_hint.column).toBeNull()
    expect(hints.notes.length).toBeGreaterThan(0)
  })
})
