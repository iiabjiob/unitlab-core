import { describe, expect, it } from "vitest"

import {
  resolveSignalGridRowKey,
  resolveSignalGridSelectedRowKeys,
} from "./rowSelection"

describe("signal grid row selection", () => {
  it("resolves signal row keys from explicit row ids or signal ids", () => {
    expect(resolveSignalGridRowKey({ row_id: "signal-7", signal_id: 7 })).toBe("signal-7")
    expect(resolveSignalGridRowKey({ rowId: "signal-8", signal_id: 8 })).toBe("signal-8")
    expect(resolveSignalGridRowKey({ signal_id: 9 })).toBe("signal-9")
    expect(resolveSignalGridRowKey({ signal_id: null })).toBeNull()
  })

  it("uses explicit selected rows for normal selection snapshots", () => {
    expect(resolveSignalGridSelectedRowKeys(
      { focusedRow: null, selectedRows: ["signal-1", "signal-3"] },
      ["signal-1", "signal-2", "signal-3"],
    )).toEqual(["signal-1", "signal-3"])
  })

  it("expands DataGrid all-mode selection against candidate row keys", () => {
    expect(resolveSignalGridSelectedRowKeys(
      { focusedRow: null, selectedRows: [], mode: "all", excludedRows: ["signal-2"] },
      ["signal-1", "signal-2", "signal-3"],
    )).toEqual(["signal-1", "signal-3"])
  })
})
