import { beforeEach, describe, expect, it, vi } from "vitest"
import { createPinia, setActivePinia } from "pinia"

import { useSignalJobStore } from "@/stores/signalJobStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { WSChannel, type SignalAllocationJobEvent, type SignalTestRunJobEvent } from "@/types/ws/events"

const signalSheetApiMock = vi.hoisted(() => ({
  getAllocationJob: vi.fn(),
  enqueueAutoAllocateJob: vi.fn(),
  enqueueBulkAllocationJob: vi.fn(),
  enqueueTestRunJob: vi.fn(),
  controlAllocationJob: vi.fn(),
}))

vi.mock("@/api/signal_sheet.api", () => ({
  SignalSheetAPI: signalSheetApiMock,
}))

function buildJobEvent(overrides: Partial<SignalTestRunJobEvent> = {}): SignalTestRunJobEvent {
  return {
    channel: WSChannel.SYSTEM_INFO,
    event: "signal_test_run_job",
    job_id: "job-1",
    workspace_id: 7,
    operation: "test_run",
    status: "running",
    progress_total: 10,
    progress_done: 0,
    message: "Signals 0/10",
    error: null,
    result: {},
    created_at: "2026-07-03T00:00:00.000Z",
    updated_at: "2026-07-03T00:00:00.000Z",
    ...overrides,
  }
}

describe("signalJobStore progress updates", () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    useWorkspaceStore().setActiveWorkspace(7)
    Object.values(signalSheetApiMock).forEach(mock => mock.mockReset())
  })

  it("applies progress chunks even when updated_at is unchanged", () => {
    const store = useSignalJobStore()

    store.applyJobEvent(buildJobEvent())
    store.applyJobEvent(buildJobEvent({
      progress_done: 4,
      message: "Signals 4/10",
    }))

    expect(store.jobsById["job-1"].progress_done).toBe(4)
    expect(store.activeJobs[0].progress_done).toBe(4)
    expect(store.progressText).toContain("4/10")
  })

  it("applies allocation progress chunks with unchanged updated_at", () => {
    const store = useSignalJobStore()

    const allocationEvent: SignalAllocationJobEvent = {
      ...buildJobEvent(),
      channel: WSChannel.SYSTEM_INFO,
      event: "signal_allocation_job",
      operation: "bulk_update",
      message: "Rows 0/10",
    }
    const allocationProgressEvent: SignalAllocationJobEvent = {
      ...buildJobEvent(),
      channel: WSChannel.SYSTEM_INFO,
      event: "signal_allocation_job",
      operation: "bulk_update",
      progress_done: 6,
      message: "Rows 6/10",
    }

    store.applyJobEvent(allocationEvent)
    store.applyJobEvent(allocationProgressEvent)

    expect(store.jobsById["job-1"].progress_done).toBe(6)
    expect(store.activeJobs[0].progress_done).toBe(6)
    expect(store.progressText).toContain("6/10")
  })

  it("does not roll active progress back when a backend batch restarts at zero", () => {
    const store = useSignalJobStore()

    store.applyJobEvent(buildJobEvent({
      progress_done: 6,
      message: "Signals 6/10",
      updated_at: "2026-07-03T00:00:06.000Z",
    }))
    store.applyJobEvent(buildJobEvent({
      progress_done: 0,
      message: "Signals 0/10",
      updated_at: "2026-07-03T00:00:07.000Z",
    }))

    expect(store.jobsById["job-1"].progress_done).toBe(6)
    expect(store.jobsById["job-1"].message).toBe("Signals 6/10")
    expect(store.progressText).toContain("6/10")
  })

  it("merges partial IEC 61850 preparation results without dropping steps", () => {
    const store = useSignalJobStore()

    store.applyJobEvent(buildJobEvent({
      result: {
        phase: "preparing_iec61850",
        verification_prepare_steps: [
          { id: "subscribe_reports", label: "Subscribe reports", status: "done" },
        ],
      },
    }))
    store.applyJobEvent(buildJobEvent({
      message: "IEC 61850 unavailable; starting test without verification.",
      result: {
        verification_prepare_error: "MMS report timeout",
      },
    }))

    expect(store.jobsById["job-1"].result.verification_prepare_steps).toEqual([
      { id: "subscribe_reports", label: "Subscribe reports", status: "done" },
    ])
    expect(store.jobsById["job-1"].result.verification_prepare_error).toBe("MMS report timeout")
  })
})
