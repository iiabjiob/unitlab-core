import { describe, expect, it } from "vitest"

import {
  resolveSignalGridColumnPatchPolicy,
  resolveSignalGridPatchPolicy,
} from "./signalGridPatchPolicy"

describe("signalGridPatchPolicy", () => {
  it("keeps runtime tested_at patches stable by default", () => {
    expect(resolveSignalGridPatchPolicy(["tested_at"], { reason: "runtime" })).toEqual({
      mode: "row",
      reason: "runtime",
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
      emit: undefined,
      immediate: undefined,
    })
  })

  it("recomputes filter/sort/group for allocation status fields unless overridden", () => {
    expect(resolveSignalGridPatchPolicy(["allocation_status", "allocation_health"])).toMatchObject({
      mode: "row",
      recomputeSort: true,
      recomputeFilter: true,
      recomputeGroup: true,
    })
    expect(resolveSignalGridPatchPolicy(["allocation_status"], {
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })).toMatchObject({
      mode: "row",
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })
  })

  it("marks future display-only runtime fields as refresh-only", () => {
    expect(resolveSignalGridColumnPatchPolicy("runtime_value")).toEqual({
      mode: "refresh",
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })
  })

  it("keeps source and unknown columns as stable row patches", () => {
    expect(resolveSignalGridColumnPatchPolicy("source_col_1")).toEqual({
      mode: "row",
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })
    expect(resolveSignalGridColumnPatchPolicy("unknown")).toEqual({
      mode: "row",
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })
  })
})
