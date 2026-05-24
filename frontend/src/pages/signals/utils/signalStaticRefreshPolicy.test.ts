import { describe, expect, it } from "vitest"

import { resolveSignalStaticRefreshReason } from "./signalStaticRefreshPolicy"

describe("resolveSignalStaticRefreshReason", () => {
  it("allows full static refresh only for explicit lifecycle triggers", () => {
    expect(resolveSignalStaticRefreshReason({ kind: "initial_load" })).toBe("initial_load")
    expect(resolveSignalStaticRefreshReason({ kind: "workspace_switch" })).toBe("workspace_switch")
    expect(resolveSignalStaticRefreshReason({ kind: "import" })).toBe("import")
    expect(resolveSignalStaticRefreshReason({ kind: "operator_manual_refresh" })).toBe("operator_manual_refresh")
  })

  it("does not request full reload for normal allocation or runtime updates", () => {
    expect(resolveSignalStaticRefreshReason({ kind: "allocation_job_completed" })).toBeNull()
    expect(resolveSignalStaticRefreshReason({ kind: "runtime_patch_burst" })).toBeNull()
    expect(resolveSignalStaticRefreshReason({
      kind: "signal_rows_patched",
      requiresFullReload: false,
      missingSignalIds: [],
    })).toBeNull()
  })

  it("treats patch stream gaps as reconnect recovery reloads", () => {
    expect(resolveSignalStaticRefreshReason({
      kind: "signal_rows_patched",
      requiresFullReload: true,
      missingSignalIds: [],
    })).toBe("reconnect_gap")
  })

  it("treats unknown row patches as recovery reloads", () => {
    expect(resolveSignalStaticRefreshReason({
      kind: "signal_rows_patched",
      requiresFullReload: false,
      missingSignalIds: [42],
    })).toBe("unknown_row_patch")
  })
})
