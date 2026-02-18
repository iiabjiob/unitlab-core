<template>
  <div
    v-if="isVisible"
    class="inline-flex items-center rounded-md border border-neutral-200 bg-neutral-50/80 text-neutral-600 dark:border-neutral-700 dark:bg-neutral-800/60 dark:text-neutral-200"
    :class="compact ? 'gap-1 px-1.5 py-1 text-[10px]' : 'gap-2 px-2 py-1 text-[11px]'"
  >
    <template v-if="activeTestRunJob">
      <template v-if="compact">
        <span class="h-1.5 w-1.5 rounded-full bg-emerald-500" :title="activeProgressText"></span>
        <span class="font-medium text-neutral-700 dark:text-neutral-100">{{ activeProgressPercent }}%</span>
      </template>
      <template v-else>
        <span class="font-medium text-neutral-700 dark:text-neutral-100">Test run</span>
        <span class="truncate max-w-[280px]">{{ activeProgressText }}</span>
        <span>{{ activeProgressPercent }}%</span>
      </template>

      <UiButton
        v-if="!compact && activeTestRunJob.status === 'running'"
        size="xs"
        variant="ghost"
        :disabled="controlBusy"
        title="Pause test run"
        @click="control('pause')"
      >
        ⏸
      </UiButton>
      <UiButton
        v-if="!compact && activeTestRunJob.status === 'paused'"
        size="xs"
        variant="ghost"
        :disabled="controlBusy"
        title="Resume test run"
        @click="control('resume')"
      >
        ▶
      </UiButton>
      <UiButton
        v-if="!compact && ['queued', 'running', 'paused'].includes(activeTestRunJob.status)"
        size="xs"
        variant="ghost"
        :disabled="controlBusy"
        title="Stop test run"
        @click="control('stop')"
      >
        ■
      </UiButton>
    </template>

    <template v-else-if="latestCompletedTestRunJob && latestCompletedTestRunJob.job_id !== dismissedJobId">
      <template v-if="compact">
        <span class="font-medium text-neutral-700 dark:text-neutral-100" :title="completedSummaryText">Last ✓</span>
      </template>
      <template v-else>
        <span class="font-medium text-neutral-700 dark:text-neutral-100">Last test</span>
        <span class="truncate max-w-[360px]">{{ completedSummaryText }}</span>
      </template>
      <button
        type="button"
        class="inline-flex h-4 w-4 items-center justify-center rounded text-neutral-500 hover:bg-neutral-200 hover:text-neutral-800 dark:text-neutral-400 dark:hover:bg-neutral-700 dark:hover:text-neutral-100"
        aria-label="Dismiss last test summary"
        @click="dismissCompleted"
      >
        ×
      </button>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { storeToRefs } from "pinia"

import UiButton from "@/components/ui/UiButton.vue"
import { useSignalAllocationJobStore } from "@/stores/signalAllocationJobStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import type { SignalAllocationJob } from "@/types/signal"

const props = withDefaults(defineProps<{ compact?: boolean }>(), {
  compact: false,
})

const workspaceStore = useWorkspaceStore()
const signalAllocationJobStore = useSignalAllocationJobStore()
const toastStore = useToastStore()

const { activeJobs, jobsById } = storeToRefs(signalAllocationJobStore)

const controlBusy = ref(false)
const dismissedJobId = ref<string | null>(null)
const compact = computed(() => Boolean(props.compact))

const activeTestRunJob = computed(() => (
  activeJobs.value.find(job => String(job.operation) === "test_run") ?? null
))

const latestCompletedTestRunJob = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) {
    return null
  }

  return Object.values(jobsById.value)
    .filter(job => job.workspace_id === workspaceId)
    .filter(job => String(job.operation) === "test_run")
    .filter(job => ["succeeded", "failed", "cancelled"].includes(String(job.status)))
    .sort((left, right) => String(right.updated_at).localeCompare(String(left.updated_at)))[0] ?? null
})

const isVisible = computed(() => Boolean(
  activeTestRunJob.value
  || (latestCompletedTestRunJob.value && latestCompletedTestRunJob.value.job_id !== dismissedJobId.value),
))

const activeProgressPercent = computed(() => {
  const job = activeTestRunJob.value
  if (!job) return 0
  const total = Math.max(0, Number(job.progress_total ?? 0))
  const done = Math.max(0, Number(job.progress_done ?? 0))
  if (!total) return 0
  return Math.max(0, Math.min(100, Math.round((Math.min(done, total) / total) * 100)))
})

const activeProgressText = computed(() => {
  const job = activeTestRunJob.value
  if (!job) return ""
  const total = Math.max(0, Number(job.progress_total ?? 0))
  const done = Math.max(0, Number(job.progress_done ?? 0))
  const msg = String(job.message ?? "").trim()
  const fallback = total > 0 ? `${done}/${total}` : (job.status === "paused" ? "paused" : "running")
  return msg || fallback
})

function readNumericResult(job: SignalAllocationJob, key: string): number {
  const raw = (job.result as Record<string, unknown> | undefined)?.[key]
  const numeric = Number(raw)
  return Number.isFinite(numeric) ? numeric : 0
}

function formatDateTimeShort(value: string): string {
  const parsed = new Date(value)
  if (Number.isNaN(parsed.getTime())) {
    return value
  }
  return new Intl.DateTimeFormat(undefined, {
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  }).format(parsed)
}

function formatDurationShort(seconds: number): string {
  const normalized = Math.max(0, Math.round(seconds))
  const minutes = Math.floor(normalized / 60)
  const remSeconds = normalized % 60
  if (minutes <= 0) {
    return `${remSeconds}s`
  }
  return `${minutes}m ${remSeconds}s`
}

const completedSummaryText = computed(() => {
  const job = latestCompletedTestRunJob.value
  if (!job) return ""

  const status = job.status === "failed"
    ? "failed"
    : job.status === "cancelled"
      ? "cancelled"
      : "completed"
  const processed = Math.max(0, readNumericResult(job, "processed") || Number(job.progress_done ?? 0))
  const succeeded = Math.max(0, readNumericResult(job, "succeeded"))

  const finishedAtParsed = Date.parse(String(job.updated_at ?? ""))
  const createdAtParsed = Date.parse(String(job.created_at ?? ""))
  const durationMs = (Number.isFinite(createdAtParsed) && Number.isFinite(finishedAtParsed))
    ? Math.max(0, finishedAtParsed - createdAtParsed)
    : 0

  return [
    status,
    formatDateTimeShort(String(job.updated_at ?? "")),
    `${succeeded}/${processed} toggled`,
    formatDurationShort(durationMs / 1000),
  ].join(" · ")
})

async function control(action: "pause" | "resume" | "stop") {
  if (controlBusy.value) return
  const workspaceId = workspaceStore.activeWorkspaceId
  const jobId = activeTestRunJob.value?.job_id
  if (!workspaceId || !jobId) {
    return
  }

  controlBusy.value = true
  try {
    await signalAllocationJobStore.controlJob(workspaceId, jobId, action)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    controlBusy.value = false
  }
}

function dismissCompleted() {
  dismissedJobId.value = latestCompletedTestRunJob.value?.job_id ?? null
}
</script>
