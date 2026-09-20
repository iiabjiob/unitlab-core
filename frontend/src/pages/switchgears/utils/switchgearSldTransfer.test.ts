import { describe, expect, it } from "vitest"

import {
  createSwitchgearSldTransfer,
  parseSwitchgearSldTransfer,
  resolveImportedBindings,
} from "./switchgearSldTransfer"

describe("switchgear SLD transfer", () => {
  it("exports hardware identity without relying on source channel ids", () => {
    const payload = createSwitchgearSldTransfer({
      workspaceId: 1,
      layoutById: { "1": { x: 10, y: 20 } },
      edges: [],
      staticElements: [],
      textElements: [],
    }, {
      switchgears: [{
        id: 1,
        workspace_ids: [1],
        switchgear_type: "switchgear",
        name: "Q1",
        bindings: [{ id: 1, role: "do_open", channel_id: 10, delay_ms: 5 }],
      }],
      channels: [{ id: 10, device_id: 7, index: 2, type: "do", name: "", resolved_name: "" }],
      resolveUnitId: () => "PLC-01",
    })

    expect(payload.schema).toBe("unitlab.sld.transfer.v1")
    expect(payload.switchgears[0]?.bindings[0]?.channel).toEqual({
      unitId: "PLC-01",
      channelIndex: 2,
      channelType: "do",
    })
    expect(parseSwitchgearSldTransfer(payload)).toEqual(payload)
  })

  it("detaches a binding when the target hardware channel is unavailable", () => {
    const result = resolveImportedBindings({
      key: "switchgear:1",
      name: "Q1",
      switchgearType: "switchgear",
      bindings: [{
        role: "do_open",
        delayMs: 0,
        channel: { unitId: "missing-plc", channelIndex: 2, channelType: "do" },
      }],
    }, [], () => "other-plc")

    expect(result.bindings).toEqual([{ role: "do_open", channel_id: null, delay_ms: 0 }])
    expect(result.detachedRoles).toEqual(["do_open"])
  })
})
