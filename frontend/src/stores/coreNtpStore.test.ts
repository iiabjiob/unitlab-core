import { beforeEach, describe, expect, it, vi } from "vitest"
import { createPinia, setActivePinia } from "pinia"
import { useCoreNtpStore } from "./coreNtpStore"
import { useToastStore } from "./toastStore"
import type { CoreNtpSnapshot } from "@/types/coreNtp"

const refresh = vi.hoisted(() => vi.fn().mockResolvedValue(undefined))
vi.mock("@/stores/systemHealthStore", () => ({ useSystemHealthStore: () => ({ refresh }) }))
vi.mock("@/api/core_ntp.api", () => ({}))

function sample(time: string, synced: boolean): CoreNtpSnapshot {
  return { mode: synced ? "ok" : "degraded", chrony_service_active: true,
    chrony_service_name: "chrony", configured_servers: [], effective_servers: [],
    system_time_utc: time, updated_at: time, tracking: { synced },
    sources: synced ? [{ name: "ntp", mode_mark: "^", state_mark: "*" }] : [] }
}

describe("NTP synchronization notice", () => {
  beforeEach(() => { setActivePinia(createPinia()); refresh.mockClear() })
  it("announces synchronization once and refreshes health", () => {
    const store = useCoreNtpStore()
    store.applySnapshot(sample("2026-01-01T00:00:00Z", false))
    const synced = sample("2026-09-16T12:00:00Z", true)
    store.applySnapshot(synced)
    store.applySnapshot(synced)
    expect(useToastStore().toasts).toHaveLength(1)
    expect(useToastStore().toasts[0]?.variant).toBe("info")
    expect(refresh).toHaveBeenCalledTimes(1)
  })
  it("does not claim synchronization on initial load or failure", () => {
    const store = useCoreNtpStore()
    store.applySnapshot(sample("2026-01-01T00:00:00Z", true))
    store.applySnapshot(sample("2026-09-16T12:00:00Z", false))
    expect(useToastStore().toasts).toHaveLength(0)
    expect(refresh).not.toHaveBeenCalled()
  })
  it.each(["2020-01-01T00:00:00Z", "2030-01-01T00:00:00Z"])("handles a synchronized clock step to %s", time => {
    const store = useCoreNtpStore()
    store.applySnapshot(sample("2026-09-16T12:00:00Z", true))
    store.applySnapshot(sample(time, true))
    expect(useToastStore().toasts).toHaveLength(1)
    expect(refresh).toHaveBeenCalledTimes(1)
  })
})
