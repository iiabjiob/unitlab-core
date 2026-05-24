import { describe, expect, it } from "vitest"

import { createSignalRuntimeStateCache } from "./signalRuntimeStateCache"

describe("createSignalRuntimeStateCache", () => {
  it("stores tested timestamps by signal id", () => {
    const cache = createSignalRuntimeStateCache()

    const result = cache.patchTestedAtBySignal({
      1: "2026-01-01T00:00:00Z",
      bad: "ignored",
    })

    expect(result).toEqual({ changed: 1, signalIds: [1] })
    expect(cache.getTestedAt(1)).toBe("2026-01-01T00:00:00Z")
    expect(cache.getTestedAt(2)).toBeNull()
    expect(cache.version).toBe(1)
  })

  it("tracks generic runtime status, value, timestamp, and reason", () => {
    const cache = createSignalRuntimeStateCache()

    cache.patchStates([
      {
        signalId: 1,
        state: {
          status: "failed",
          value: 42,
          updatedAt: "2026-01-01T00:00:01Z",
          reason: "offline",
        },
      },
    ])

    expect(cache.getState(1)).toEqual({
      status: "failed",
      value: 42,
      updatedAt: "2026-01-01T00:00:01Z",
      reason: "offline",
    })
  })

  it("does not bump version for duplicate patches", () => {
    const cache = createSignalRuntimeStateCache()
    cache.patchTestedAtBySignal({ 1: "2026-01-01T00:00:00Z" })
    const version = cache.version

    const result = cache.patchTestedAtBySignal({ 1: "2026-01-01T00:00:00Z" })

    expect(result).toEqual({ changed: 0, signalIds: [] })
    expect(cache.version).toBe(version)
  })

  it("clears runtime state without touching static projection data", () => {
    const cache = createSignalRuntimeStateCache()
    cache.patchStates([{ signalId: 1, state: { testedAt: "2026-01-01T00:00:00Z" } }])
    const version = cache.version

    cache.clear()

    expect(cache.size).toBe(0)
    expect(cache.getTestedAt(1)).toBeNull()
    expect(cache.version).toBe(version + 1)
  })
})
