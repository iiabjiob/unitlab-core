import { defineStore } from "pinia"
import { computed, ref } from "vue"

import { SignalSheetAPI } from "@/api/signal_sheet.api"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import type { SignalAllocationJob, SignalAutoAllocatePayload, SignalAllocationUpdateItem } from "@/types/signal"
import type { SignalAllocationJobEvent } from "@/types/ws/events"


type Waiter = {
  resolve: (job: SignalAllocationJob) => void
  reject: (error: Error) => void
  timer: ReturnType<typeof setTimeout>
}

const STATUS_RANK: Record<string, number> = {
  queued: 1,
  running: 2,
  succeeded: 3,
  failed: 3,
}

function statusRank(status: string): number {
  return STATUS_RANK[status] ?? 0
}

function isTerminalStatus(status: string): boolean {
  return status === "succeeded" || status === "failed"
}

function toMillis(value: string | null | undefined): number {
  if (!value) return 0
  const parsed = Date.parse(value)
  return Number.isFinite(parsed) ? parsed : 0
}

export const useSignalAllocationJobStore = defineStore("signalAllocationJobStore", () => {
  const workspaceStore = useWorkspaceStore()
  const jobsById = ref<Record<string, SignalAllocationJob>>({})
  const waiters = new Map<string, Waiter>()

  const activeWorkspaceId = computed(() => workspaceStore.activeWorkspaceId)

  const activeJobs = computed(() => (
    Object.values(jobsById.value)
      .filter((job) => {
        if (!activeWorkspaceId.value) {
          return false
        }
        return job.workspace_id === activeWorkspaceId.value
      })
      .filter(job => job.status === "queued" || job.status === "running")
      .sort((left, right) => String(right.updated_at).localeCompare(String(left.updated_at)))
  ))

  const isAnyRunning = computed(() => activeJobs.value.length > 0)

  const progressText = computed(() => {
    const top = activeJobs.value[0]
    if (!top) return null
    const total = Math.max(0, Number(top.progress_total ?? 0))
    const done = Math.max(0, Number(top.progress_done ?? 0))
    const stateLabel = top.status === "queued" ? "Queued" : "Running"
    const message = String(top.message ?? "").trim()
    if (total <= 0) {
      return message || stateLabel
    }
    const safeDone = Math.min(done, total)
    const percent = Math.max(0, Math.min(100, Math.round((safeDone / total) * 100)))
    const detail = `${percent}% · ${safeDone}/${total}`
    if (message && message.toLowerCase() !== stateLabel.toLowerCase()) {
      return `${message} · ${detail}`
    }
    return `${stateLabel} ${detail}`
  })

  function shouldApplyJobUpdate(existing: SignalAllocationJob | undefined, next: SignalAllocationJob): boolean {
    if (!existing) {
      return true
    }

    const existingRank = statusRank(existing.status)
    const nextRank = statusRank(next.status)
    if (nextRank < existingRank) {
      return false
    }
    if (nextRank > existingRank) {
      return true
    }

    const existingUpdatedAt = toMillis(existing.updated_at)
    const nextUpdatedAt = toMillis(next.updated_at)
    return nextUpdatedAt >= existingUpdatedAt
  }

  function compactFinishedJobs(maxItems = 120) {
    const allJobs = Object.values(jobsById.value)
    if (allJobs.length <= maxItems) {
      return
    }
    const sorted = allJobs
      .sort((left, right) => toMillis(right.updated_at) - toMillis(left.updated_at))
      .slice(0, maxItems)
    jobsById.value = Object.fromEntries(sorted.map(job => [job.job_id, job]))
  }

  function upsertJob(job: SignalAllocationJob) {
    const existing = jobsById.value[job.job_id]
    if (!shouldApplyJobUpdate(existing, job)) {
      return
    }

    const mergedResult = (
      Object.keys(job.result ?? {}).length > 0
        ? (job.result ?? {})
        : (existing?.result ?? {})
    )

    jobsById.value = {
      ...jobsById.value,
      [job.job_id]: {
        ...existing,
        ...job,
        result: mergedResult,
      },
    }

    compactFinishedJobs()

    if (job.status !== "succeeded" && job.status !== "failed") {
      return
    }

    const waiter = waiters.get(job.job_id)
    if (!waiter) {
      return
    }

    clearTimeout(waiter.timer)
    waiters.delete(job.job_id)
    if (job.status === "succeeded") {
      waiter.resolve(job)
      return
    }
    waiter.reject(new Error(job.error || job.message || "Signal allocation job failed"))
  }

  async function reconcileJobOnce(workspaceId: number, jobId: string): Promise<SignalAllocationJob | null> {
    try {
      const { data } = await SignalSheetAPI.getAllocationJob(workspaceId, jobId)
      upsertJob(data)
      return data
    } catch {
      return null
    }
  }

  function applyJobEvent(event: SignalAllocationJobEvent) {
    upsertJob({
      job_id: event.job_id,
      workspace_id: event.workspace_id,
      operation: event.operation,
      status: event.status,
      progress_total: Number(event.progress_total ?? 0),
      progress_done: Number(event.progress_done ?? 0),
      message: event.message ?? null,
      error: event.error ?? null,
      result: event.result ?? {},
      created_at: event.created_at,
      updated_at: event.updated_at,
    })
  }

  function awaitJobCompletion(jobId: string, workspaceId: number, timeoutMs = 90_000): Promise<SignalAllocationJob> {
    const existing = jobsById.value[jobId]
    if (existing?.status === "succeeded") {
      return Promise.resolve(existing)
    }
    if (existing?.status === "failed") {
      return Promise.reject(new Error(existing.error || existing.message || "Signal allocation job failed"))
    }

    return new Promise<SignalAllocationJob>((resolve, reject) => {
      const timer = setTimeout(() => {
        void (async () => {
          waiters.delete(jobId)

          const refreshed = await reconcileJobOnce(workspaceId, jobId)
          if (refreshed?.status === "succeeded") {
            resolve(refreshed)
            return
          }
          if (refreshed?.status === "failed") {
            reject(new Error(refreshed.error || refreshed.message || "Signal allocation job failed"))
            return
          }

          const snapshot = refreshed ?? jobsById.value[jobId]
          const suffix = snapshot ? ` (last status: ${snapshot.status})` : ""
          reject(new Error(`Signal allocation job update timeout${suffix}`))
        })()
      }, timeoutMs)

      waiters.set(jobId, { resolve, reject, timer })
    })
  }

  async function enqueueAutoAllocateJob(workspaceId: number, payload: SignalAutoAllocatePayload): Promise<SignalAllocationJob> {
    const { data: queuedJob } = await SignalSheetAPI.enqueueAutoAllocateJob(workspaceId, payload)
    upsertJob(queuedJob)
    return await awaitJobCompletion(queuedJob.job_id, workspaceId)
  }

  async function enqueueBulkUpdateJob(workspaceId: number, entries: SignalAllocationUpdateItem[]): Promise<SignalAllocationJob> {
    const { data: queuedJob } = await SignalSheetAPI.enqueueBulkAllocationJob(workspaceId, entries)
    upsertJob(queuedJob)
    return await awaitJobCompletion(queuedJob.job_id, workspaceId)
  }

  async function enqueueTestRunJob(
    workspaceId: number,
    signalIds: number[],
    toggleStepMs = 1000,
  ): Promise<SignalAllocationJob> {
    const payload = {
      signal_ids: signalIds,
      toggle_step_ms: toggleStepMs,
    }
    const { data: queuedJob } = await SignalSheetAPI.enqueueTestRunJob(workspaceId, payload)
    upsertJob(queuedJob)
    return await awaitJobCompletion(queuedJob.job_id, workspaceId, 10 * 60_000)
  }

  function clearWorkspaceJobs(workspaceId: number) {
    const next: Record<string, SignalAllocationJob> = {}
    Object.values(jobsById.value).forEach((job) => {
      if (job.workspace_id !== workspaceId) {
        next[job.job_id] = job
      }
    })
    jobsById.value = next
  }

  return {
    jobsById,
    activeJobs,
    isAnyRunning,
    progressText,
    applyJobEvent,
    enqueueAutoAllocateJob,
    enqueueBulkUpdateJob,
    enqueueTestRunJob,
    clearWorkspaceJobs,
  }
})
