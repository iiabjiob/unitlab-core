<template>
  <div v-if="isVisible" class="global-signal-test-status">
    <template v-if="activeTestRunJob">
      <div class="global-signal-test-status__test-run">
        <GlobalProgressStatusCard
          :compact="compact"
          interactive
          label="Test run"
          :percent="activeProgressPercent"
          :detail="activeProgressDetailText"
          @click="navigateToSignals"
        >
          <template #actions>
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
        </GlobalProgressStatusCard>

        <div v-if="showVerificationPrepareDetails" class="global-signal-test-status__prepare">
          <div class="global-signal-test-status__prepare-steps">
            <span
              v-for="step in verificationPrepareSteps"
              :key="step.id"
              class="global-signal-test-status__prepare-step"
            >
              <span class="global-signal-test-status__prepare-dot" :class="`is-${step.status}`"></span>
              <span class="global-signal-test-status__prepare-label">{{ step.label }}</span>
              <span v-if="step.detail" class="global-signal-test-status__prepare-detail">{{ step.detail }}</span>
            </span>
          </div>
          <div v-if="verificationPrepareSubscriptions.length" class="global-signal-test-status__prepare-subscriptions">
            <span
              v-for="subscription in verificationPrepareSubscriptions"
              :key="subscription.id"
              class="global-signal-test-status__prepare-subscription"
              :title="subscription.title"
            >
              <span class="global-signal-test-status__prepare-label">{{ subscription.report }}</span>
              <span>{{ subscription.state }}</span>
              <span>{{ subscription.gi }}</span>
              <span>{{ subscription.values }}</span>
            </span>
          </div>
        </div>
      </div>
    </template>

    <template v-if="activeAllocationJob">
      <GlobalProgressStatusCard
        :compact="compact"
        interactive
        :label="activeAllocationLabel"
        :percent="activeAllocationProgressPercent"
        :detail="activeAllocationDetailText"
        :dot-class="activeAllocationDotClass"
        :bar-class="activeAllocationBarClass"
        @click="navigateToSignals"
      />
    </template>

    <template v-if="!activeTestRunJob && !activeAllocationJob && latestCompletedTestRunJob && latestCompletedTestRunJob.job_id !== dismissedJobId">
      <div
        class="global-signal-test-status__completed"
        :class="compact ? 'global-signal-test-status__completed--compact' : 'global-signal-test-status__completed--regular'"
      >
        <template v-if="compact">
          <span class="global-signal-test-status__completed-title" :title="completedSummaryText">Last ✓</span>
        </template>
        <template v-else>
          <span class="global-signal-test-status__completed-title">Last test</span>
          <span class="global-signal-test-status__completed-summary">{{ completedSummaryText }}</span>
        </template>
        <button
          type="button"
          class="global-signal-test-status__dismiss"
          aria-label="Dismiss last test summary"
          @click="dismissCompleted"
        >
          ×
        </button>
      </div>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { storeToRefs } from "pinia"
import { useRouter } from "vue-router"

import UiButton from "@/components/ui/UiButton.vue"
import GlobalProgressStatusCard from "./GlobalProgressStatusCard.vue"
import { useSignalJobStore } from "@/stores/signalJobStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import type { SignalAllocationJob } from "@/types/signal"

const props = withDefaults(defineProps<{ compact?: boolean }>(), {
  compact: false,
})

const router = useRouter()
const workspaceStore = useWorkspaceStore()
const signalJobStore = useSignalJobStore()
const toastStore = useToastStore()

const { activeJobs, jobsById } = storeToRefs(signalJobStore)

const controlBusy = ref(false)
const dismissedJobId = ref<string | null>(null)
const compact = computed(() => Boolean(props.compact))

type VerificationPrepareStepView = {
  id: string
  label: string
  status: string
  detail: string
}

type VerificationPrepareSubscriptionView = {
  id: string
  report: string
  state: string
  gi: string
  values: string
  title: string
}

const activeTestRunJob = computed(() => (
  activeJobs.value.find(job => String(job.operation) === "test_run") ?? null
))

const activeAllocationJob = computed(() => {
  const candidates = activeJobs.value
    .filter(job => ["auto_allocate", "bulk_update"].includes(String(job.operation)))
    .sort((left, right) => toMillis(String(right.updated_at ?? "")) - toMillis(String(left.updated_at ?? "")))
  return candidates[0] ?? null
})

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
  || activeAllocationJob.value
  || (latestCompletedTestRunJob.value && latestCompletedTestRunJob.value.job_id !== dismissedJobId.value),
))

const activeProgressPercent = computed(() => {
  const job = activeTestRunJob.value
  if (!job) return 0
  const { done, total } = resolveActiveProgress(job)
  if (!total) return 0
  return Math.max(0, Math.min(100, Math.round((Math.min(done, total) / total) * 100)))
})

const activeProgressText = computed(() => {
  const job = activeTestRunJob.value
  if (!job) return ""
  const { done, total } = resolveActiveProgress(job)
  const msg = String(job.message ?? "").trim()
  const fallback = total > 0 ? `${done}/${total}` : (job.status === "paused" ? "paused" : "running")
  return msg || fallback
})

const activeProgressDetailText = computed(() => {
  const job = activeTestRunJob.value
  if (!job) return ""
  const { done, total } = resolveActiveProgress(job)
  const message = String(job.message ?? "").trim()
  if (total <= 0) {
    return message || activeProgressText.value
  }

  const succeeded = Math.max(0, readNumericResult(job, "succeeded"))
  const skipped = Math.max(0, readNumericResult(job, "skipped"))
  const resumeMeta = formatResumeMeta(job)
  const base = `${done}/${total} · ok ${succeeded} · skip ${skipped}${resumeMeta ? ` · ${resumeMeta}` : ""}`
  const detail = activeEstText.value ? `${base} · ${activeEstText.value}` : base
  if (message && !message.includes(base)) {
    return `${message} · ${detail}`
  }
  return detail
})

const verificationPrepareSteps = computed<VerificationPrepareStepView[]>(() => {
  const job = activeTestRunJob.value
  const raw = (job?.result as Record<string, unknown> | undefined)?.verification_prepare_steps
  if (!Array.isArray(raw)) {
    return []
  }
  return raw
    .map((item): VerificationPrepareStepView | null => {
      if (!item || typeof item !== "object") {
        return null
      }
      const payload = item as Record<string, unknown>
      const id = String(payload.id ?? "").trim()
      const label = String(payload.label ?? "").trim()
      if (!id || !label) {
        return null
      }
      return {
        id,
        label,
        status: normalizePrepareStatus(payload.status),
        detail: String(payload.detail ?? "").trim(),
      }
    })
    .filter((item): item is VerificationPrepareStepView => Boolean(item))
})

const verificationPrepareSubscriptions = computed<VerificationPrepareSubscriptionView[]>(() => {
  const job = activeTestRunJob.value
  const raw = (job?.result as Record<string, unknown> | undefined)?.verification_prepare_subscriptions
  if (!Array.isArray(raw)) {
    return []
  }
  return raw
    .map((item): VerificationPrepareSubscriptionView | null => {
      if (!item || typeof item !== "object") {
        return null
      }
      const payload = item as Record<string, unknown>
      const id = String(payload.subscription_id ?? "").trim()
      if (!id) {
        return null
      }
      const report = String(payload.report_control_name ?? payload.report_control_reference ?? "Report").trim()
      const dataset = String(payload.data_set_reference ?? "").trim()
      const state = String(payload.subscription_state ?? "pending").trim()
      const giRequested = Boolean(payload.gi_requested)
      const valueCount = Math.max(0, Number(payload.last_report_value_count ?? 0))
      return {
        id,
        report,
        state,
        gi: giRequested ? "GI sent" : "GI pending",
        values: `${valueCount} value${valueCount === 1 ? "" : "s"}`,
        title: [report, dataset, state].filter(Boolean).join(" · "),
      }
    })
    .filter((item): item is VerificationPrepareSubscriptionView => Boolean(item))
})

const showVerificationPrepareDetails = computed(() => (
  !compact.value
  && activeTestRunJob.value?.status === "running"
  && (verificationPrepareSteps.value.length > 0 || verificationPrepareSubscriptions.value.length > 0)
))

const activeEstText = computed(() => {
  const job = activeTestRunJob.value
  if (!job) return ""
  const { done, total } = resolveActiveProgress(job)
  if (total <= 0 || done <= 0 || done >= total) {
    return ""
  }

  const startedAtRaw = (job as unknown as Record<string, unknown>)?.started_at
  const startedAtMs = toMillis(String(startedAtRaw ?? "")) || toMillis(String(job.created_at ?? ""))
  if (!startedAtMs) {
    return ""
  }

  const elapsedSeconds = Math.max(1, Math.round((Date.now() - startedAtMs) / 1000))
  const rate = done / elapsedSeconds
  if (!Number.isFinite(rate) || rate <= 0) {
    return ""
  }

  const remaining = Math.max(0, total - done)
  if (remaining <= 0) {
    return ""
  }

  const estimatedSeconds = Math.max(1, Math.round(remaining / rate))
  return `ETA ${formatDurationShort(estimatedSeconds)}`
})

const activeAllocationLabel = computed(() => {
  const job = activeAllocationJob.value
  if (!job) return "Signals"
  return String(job.operation) === "bulk_update" ? "Unassign" : "Assign"
})

const activeAllocationProgress = computed(() => {
  const job = activeAllocationJob.value
  return job ? resolveActiveProgress(job) : { done: 0, total: 0 }
})

const activeAllocationProgressPercent = computed(() => {
  const { done, total } = activeAllocationProgress.value
  if (!total) return 0
  return Math.max(0, Math.min(100, Math.round((Math.min(done, total) / total) * 100)))
})

const activeAllocationDetailText = computed(() => {
  const job = activeAllocationJob.value
  if (!job) return ""
  const { done, total } = activeAllocationProgress.value
  const message = String(job.message ?? "").trim()
  const ratio = total > 0 ? `${done}/${total}` : ""
  if (message && ratio && !message.includes(ratio)) {
    return `${message} · ${ratio}`
  }
  return message || ratio || String(job.status)
})

const activeAllocationDotClass = computed(() => {
  const job = activeAllocationJob.value
  if (!job) return "global-progress-card__tone--success"
  return String(job.operation) === "bulk_update"
    ? "global-progress-card__tone--warning"
    : "global-progress-card__tone--info"
})

const activeAllocationBarClass = computed(() => activeAllocationDotClass.value)

function resolveActiveProgress(job: SignalAllocationJob): { done: number; total: number } {
  let total = Math.max(0, Number(job.progress_total ?? 0))
  let done = Math.max(0, Number(job.progress_done ?? 0))

  const processed = readNumericResult(job, "processed")
  if (processed > 0) {
    done = Math.max(done, processed)
  }

  const message = String(job.message ?? "")
  const ratioMatch = message.match(/(\d+)\s*\/\s*(\d+)/)
  if (ratioMatch) {
    const parsedDone = Number(ratioMatch[1] ?? 0)
    const parsedTotal = Number(ratioMatch[2] ?? 0)
    if (Number.isFinite(parsedDone) && parsedDone >= 0) {
      done = Math.max(done, parsedDone)
    }
    if (Number.isFinite(parsedTotal) && parsedTotal > 0) {
      total = Math.max(total, parsedTotal)
    }
  }

  if (total > 0 && done > total) {
    done = total
  }

  return { done, total }
}

function readNumericResult(job: SignalAllocationJob, key: string): number {
  const raw = (job.result as Record<string, unknown> | undefined)?.[key]
  const numeric = Number(raw)
  return Number.isFinite(numeric) ? numeric : 0
}

function readStringResult(job: SignalAllocationJob, key: string): string {
  const raw = (job.result as Record<string, unknown> | undefined)?.[key]
  return String(raw ?? "").trim()
}

function normalizePrepareStatus(value: unknown): string {
  const status = String(value ?? "").trim().toLowerCase()
  if (["done", "running", "pending", "failed"].includes(status)) {
    return status
  }
  return "pending"
}

function formatResumeMeta(job: SignalAllocationJob): string {
  const result = (job.result as Record<string, unknown> | undefined) ?? {}
  const resumeApplied = Boolean(result.resume_applied ?? result.resumed_from_cursor)
  const resumeOffset = Number(result.resume_offset ?? 0)
  const cursorReason = readStringResult(job, "cursor_reason")
  if (resumeApplied && Number.isFinite(resumeOffset) && resumeOffset > 0) {
    return `resumed ${resumeOffset}`
  }
  if (!resumeApplied && cursorReason && cursorReason !== "disabled") {
    return `resume ${cursorReason.replace(/_/g, " ")}`
  }
  return ""
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

function toMillis(value: string): number {
  const parsed = Date.parse(String(value ?? ""))
  return Number.isFinite(parsed) ? parsed : 0
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
  const skipped = Math.max(0, readNumericResult(job, "skipped"))
  const total = Math.max(
    processed,
    Math.max(0, Number(job.progress_total ?? 0)),
    succeeded + skipped,
  )
  const resumeMeta = formatResumeMeta(job)

  const finishedAtParsed = Date.parse(String(job.updated_at ?? ""))
  const createdAtParsed = Date.parse(String(job.created_at ?? ""))
  const durationMs = (Number.isFinite(createdAtParsed) && Number.isFinite(finishedAtParsed))
    ? Math.max(0, finishedAtParsed - createdAtParsed)
    : 0

  return [
    status,
    formatDateTimeShort(String(job.updated_at ?? "")),
    `ok ${succeeded} · skip ${skipped} · total ${total}`,
    ...(resumeMeta ? [resumeMeta] : []),
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
    await signalJobStore.controlJob(workspaceId, jobId, action)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    controlBusy.value = false
  }
}

function navigateToSignals() {
  void router.push({ name: "signals.home" }).catch(() => {
    return
  })
}

function dismissCompleted() {
  dismissedJobId.value = latestCompletedTestRunJob.value?.job_id ?? null
}
</script>

<style scoped>
.global-signal-test-status {
  display: flex;
  align-items: center;
  gap: 0.375rem;
}

.global-signal-test-status__test-run {
  display: flex;
  max-width: min(72vw, 820px);
  align-items: center;
  gap: 0.375rem;
}

.global-signal-test-status__prepare {
  display: flex;
  min-width: 0;
  max-width: 520px;
  flex-direction: column;
  gap: 0.125rem;
  color: var(--color-neutral-600);
  font-size: 10px;
  line-height: 1.25;
}

.global-signal-test-status__prepare-steps,
.global-signal-test-status__prepare-subscriptions {
  display: flex;
  min-width: 0;
  gap: 0.25rem;
  overflow: hidden;
  white-space: nowrap;
}

.global-signal-test-status__prepare-step,
.global-signal-test-status__prepare-subscription {
  display: inline-flex;
  min-width: 0;
  max-width: 220px;
  flex: 0 1 auto;
  align-items: center;
  gap: 0.25rem;
  overflow: hidden;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-sm);
  padding: 0.125rem 0.25rem;
  background: color-mix(in srgb, var(--color-neutral-50) 72%, transparent);
  text-overflow: ellipsis;
}

.global-signal-test-status__prepare-dot {
  width: 0.375rem;
  height: 0.375rem;
  flex: 0 0 0.375rem;
  border-radius: 9999px;
  background: var(--color-neutral-400);
}

.global-signal-test-status__prepare-dot.is-done {
  background: var(--color-emerald-500);
}

.global-signal-test-status__prepare-dot.is-running {
  background: var(--color-sky-500);
}

.global-signal-test-status__prepare-dot.is-failed {
  background: var(--color-red-500);
}

.global-signal-test-status__prepare-label {
  min-width: 0;
  overflow: hidden;
  color: var(--color-neutral-700);
  font-weight: 500;
  text-overflow: ellipsis;
}

.global-signal-test-status__prepare-detail {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.global-signal-test-status__completed {
  display: inline-flex;
  align-items: center;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-neutral-50) 80%, transparent);
  color: var(--color-neutral-600);
}

.global-signal-test-status__completed--compact {
  gap: 0.25rem;
  padding: 0.25rem 0.375rem;
  font-size: 10px;
  line-height: 1.2;
}

.global-signal-test-status__completed--regular {
  gap: 0.5rem;
  padding: 0.25rem 0.5rem;
  font-size: 11px;
  line-height: 1.25;
}

.global-signal-test-status__completed-title {
  color: var(--color-neutral-700);
  font-weight: 500;
}

.global-signal-test-status__completed-summary {
  max-width: 360px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.global-signal-test-status__dismiss {
  display: inline-flex;
  width: 1rem;
  height: 1rem;
  align-items: center;
  justify-content: center;
  padding: 0;
  border: 0;
  border-radius: var(--radius-sm);
  background: transparent;
  color: var(--color-neutral-500);
  font: inherit;
  line-height: 1;
}

.global-signal-test-status__dismiss:hover {
  background: var(--color-neutral-200);
  color: var(--color-neutral-800);
}

:global(.dark .global-signal-test-status__completed){
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-800) 60%, transparent);
  color: var(--color-neutral-200);
}

:global(.dark .global-signal-test-status__prepare) {
  color: var(--color-neutral-300);
}

:global(.dark .global-signal-test-status__prepare-step),
:global(.dark .global-signal-test-status__prepare-subscription) {
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-800) 60%, transparent);
}

:global(.dark .global-signal-test-status__prepare-label) {
  color: var(--color-neutral-100);
}

:global(.dark .global-signal-test-status__completed-title){
  color: var(--color-neutral-100);
}

:global(.dark .global-signal-test-status__dismiss){
  color: var(--color-neutral-400);
}

:global(.dark .global-signal-test-status__dismiss:hover){
  background: var(--color-neutral-700);
  color: var(--color-neutral-100);
}
</style>
