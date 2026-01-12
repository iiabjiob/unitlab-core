<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <div
      v-if="workspaceMissing"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 px-6 py-10 text-center text-sm text-neutral-500 dark:border-neutral-700 dark:text-neutral-300"
    >
      Select a workspace to inspect test runs.
    </div>

    <div v-else class="flex h-full flex-col gap-4">
      <div
        v-if="runsLoading && !run"
        class="flex flex-1 items-center justify-center rounded-2xl border border-neutral-200 bg-white text-sm text-neutral-500 dark:border-neutral-800 dark:bg-neutral-900"
      >
        Loading test run…
      </div>

      <div
        v-else-if="!run"
        class="flex flex-1 flex-col items-center justify-center gap-4 rounded-2xl border border-neutral-200 bg-white text-center text-sm dark:border-neutral-800 dark:bg-neutral-900"
      >
        <div>
          <p class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Test run not found</p>
          <p class="text-sm text-neutral-500">It may have been deleted or belongs to another workspace.</p>
        </div>
        <RouterLink class="btn-primary" :to="{ name: 'testRuns.new' }">Create new run</RouterLink>
      </div>

      <div v-else class="flex flex-1 flex-col gap-4">
        <header class="flex flex-col gap-4 rounded-2xl border border-neutral-200 bg-white p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900 lg:flex-row lg:items-center lg:justify-between">
          <div class="flex flex-col gap-2">
            <p class="text-xs font-semibold uppercase tracking-[0.3em] text-neutral-400">Test Run #{{ run.id }}</p>
            <h1 class="text-2xl font-semibold text-neutral-900 dark:text-neutral-50">{{ runTitle }}</h1>
            <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500">
              <UiBadge :variant="statusVariant(run.status)">{{ run.status }}</UiBadge>
              <UiBadge :variant="run.mode === 'signal' ? 'info' : 'warning'" class="uppercase">{{ run.mode }} mode</UiBadge>
              <span>Created {{ formatDate(run.created_at) }}</span>
              <span v-if="run.started_at">Started {{ formatDate(run.started_at) }}</span>
              <span v-if="run.finished_at">Finished {{ formatDate(run.finished_at) }}</span>
            </div>
          </div>
          <div class="flex flex-wrap gap-3">
            <button class="btn-tertiary" type="button" :disabled="!!actionLoading" @click="repeatRun">
              <span v-if="actionLoading === 'repeat'">Cloning…</span>
              <span v-else>Clone run</span>
            </button>
            <button class="btn-secondary" type="button" :disabled="runsLoading" @click="refresh">Refresh</button>
            <button
              class="btn-primary"
              type="button"
              :disabled="!!actionLoading || run.status === 'running'"
              @click="startRun"
            >
              <span v-if="actionLoading === 'start'">Starting…</span>
              <span v-else>Start run</span>
            </button>
            <button
              class="btn-tertiary"
              type="button"
              :disabled="!!actionLoading || run.status !== 'running'"
              @click="stopRun"
            >
              <span v-if="actionLoading === 'stop'">Stopping…</span>
              <span v-else>Stop run</span>
            </button>
          </div>
        </header>

        <div class="grid flex-1 grid-cols-1 gap-4 xl:grid-cols-3">
          <section class="xl:col-span-2 rounded-2xl border border-neutral-200 bg-white p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
            <div class="flex items-center justify-between">
              <div>
                <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">Sequences</p>
                <p class="text-xs text-neutral-500">Ordered list of sequences queued inside this run.</p>
              </div>
              <RouterLink class="btn-tertiary" :to="{ name: 'testRuns.new' }">Add another run</RouterLink>
            </div>
            <div v-if="!run.sequence_ids.length" class="mt-6 rounded-xl bg-neutral-50 px-4 py-6 text-sm text-neutral-500 dark:bg-neutral-800">
              No sequences attached.
            </div>
            <ol v-else class="mt-6 space-y-3">
              <li
                v-for="(sequenceId, idx) in run.sequence_ids"
                :key="sequenceId"
                class="flex items-center gap-3 rounded-xl border border-neutral-100 bg-neutral-50 px-4 py-3 text-sm dark:border-neutral-800 dark:bg-neutral-800"
              >
                <span class="flex h-6 w-6 items-center justify-center rounded-full bg-neutral-900 text-xs font-semibold text-white dark:bg-white dark:text-neutral-900">
                  {{ idx + 1 }}
                </span>
                <div>
                  <p class="font-medium text-neutral-900 dark:text-neutral-50">{{ describeSequence(sequenceId) }}</p>
                  <p class="text-xs text-neutral-500">Sequence #{{ sequenceId }}</p>
                </div>
              </li>
            </ol>
          </section>

          <section class="rounded-2xl border border-neutral-200 bg-white p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
            <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">Run metadata</p>
            <dl class="mt-4 space-y-3 text-sm">
              <div class="flex justify-between gap-4">
                <dt class="text-neutral-500">Workspace</dt>
                <dd class="text-neutral-900 dark:text-neutral-100">{{ workspaceStore.activeWorkspace?.name ?? "—" }}</dd>
              </div>
              <div class="flex justify-between gap-4">
                <dt class="text-neutral-500">Snapshot</dt>
                <dd class="text-neutral-900 dark:text-neutral-100">{{ run.signal_snapshot_id ?? "—" }}</dd>
              </div>
              <div class="flex justify-between gap-4">
                <dt class="text-neutral-500">Created</dt>
                <dd class="text-neutral-900 dark:text-neutral-100">{{ formatDate(run.created_at) }}</dd>
              </div>
              <div class="flex justify-between gap-4" v-if="run.started_at">
                <dt class="text-neutral-500">Started</dt>
                <dd class="text-neutral-900 dark:text-neutral-100">{{ formatDate(run.started_at) }}</dd>
              </div>
              <div class="flex justify-between gap-4" v-if="run.finished_at">
                <dt class="text-neutral-500">Finished</dt>
                <dd class="text-neutral-900 dark:text-neutral-100">{{ formatDate(run.finished_at) }}</dd>
              </div>
              <div>
                <dt class="text-neutral-500 text-sm">Notes</dt>
                <dd class="mt-1 rounded-xl bg-neutral-50 px-3 py-2 text-sm text-neutral-800 dark:bg-neutral-800 dark:text-neutral-100">
                  {{ run.allocation?.notes ?? "No operator notes" }}
                </dd>
              </div>
            </dl>
          </section>
        </div>

        <section class="rounded-2xl border border-neutral-200 bg-white p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
          <div>
            <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">Allocation entries</p>
            <p class="text-xs text-neutral-500">Signal bindings applied to this run.</p>
          </div>

          <div v-if="!allocationEntries.length" class="mt-6 rounded-xl bg-neutral-50 px-4 py-6 text-sm text-neutral-500 dark:bg-neutral-800">
            No allocation entries attached.
          </div>

          <div v-else class="mt-4 overflow-x-auto">
            <table class="min-w-full divide-y divide-neutral-200 text-sm dark:divide-neutral-800">
              <thead>
                <tr>
                  <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Channel</th>
                  <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Signal key</th>
                  <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Metadata</th>
                </tr>
              </thead>
              <tbody class="divide-y divide-neutral-100 dark:divide-neutral-800">
                <tr v-for="entry in allocationEntries" :key="entry.id">
                  <td class="px-3 py-2 font-medium text-neutral-900 dark:text-neutral-50">Channel {{ entry.channel_id }}</td>
                  <td class="px-3 py-2 text-neutral-600 dark:text-neutral-200">{{ entry.signal_key ?? "—" }}</td>
                  <td class="px-3 py-2 text-neutral-600 dark:text-neutral-200">
                    <pre class="whitespace-pre-wrap text-xs">{{ formatMetadata(entry.signal_metadata) }}</pre>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </section>

        <section v-if="run.execution_meta" class="rounded-2xl border border-neutral-200 bg-white p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
          <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">Execution metadata</p>
          <pre class="mt-4 overflow-x-auto rounded-xl bg-neutral-50 px-4 py-4 text-xs text-neutral-700 dark:bg-neutral-800 dark:text-neutral-100">
{{ formattedExecutionMeta }}
          </pre>
        </section>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { RouterLink, useRouter } from "vue-router"

import UiBadge from "@/components/ui/UiBadge.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import type { TestRun } from "@/types/signal"
import type { SequenceDef } from "@/types/sequences"
import { formatDate } from "@/utils/datetime"

const props = defineProps<{ runId: number | null }>()

const snapshotStore = useSignalSnapshotStore()
const sequenceStore = useSequenceStore()
const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()
const router = useRouter()

const actionLoading = ref<"start" | "stop" | "repeat" | null>(null)

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const runsLoading = computed(() => snapshotStore.runsLoading)

const run = computed<TestRun | null>(() => snapshotStore.runs.find(r => r.id === props.runId) ?? null)

const sequenceMap = computed(() => {
  const map = new Map<number, SequenceDef>()
  sequenceStore.sequences.forEach(seq => map.set(seq.id, seq))
  return map
})

const runTitle = computed(() => {
  if (!run.value) return "—"
  if (run.value.allocation?.notes) {
    return run.value.allocation.notes.length > 64
      ? `${run.value.allocation.notes.slice(0, 61)}…`
      : run.value.allocation.notes
  }
  return `Run with ${run.value.sequence_ids.length} sequence(s)`
})

const allocationEntries = computed(() => run.value?.allocation?.entries ?? [])

const formattedExecutionMeta = computed(() =>
  run.value?.execution_meta ? JSON.stringify(run.value.execution_meta, null, 2) : "{}",
)

onMounted(() => {
  if (!workspaceMissing.value) {
    void initialize()
  }
})

watch(
  () => workspaceStore.activeWorkspaceId,
  async (value) => {
    if (!value) return
    await initialize()
  },
)

watch(
  () => props.runId,
  async (runId, prev) => {
    if (runId && runId !== prev && !run.value) {
      await snapshotStore.refreshRuns()
    }
  },
)

async function initialize() {
  await Promise.allSettled([
    snapshotStore.refreshRuns(),
    sequenceStore.ensureLoaded(),
  ])
}

function describeSequence(sequenceId: number) {
  const seq = sequenceMap.value.get(sequenceId)
  return seq ? `${seq.name}` : `Sequence #${sequenceId}`
}

function formatMetadata(meta: Record<string, unknown> | null | undefined) {
  if (!meta) return "—"
  try {
    return JSON.stringify(meta, null, 2)
  } catch {
    return String(meta)
  }
}

function statusVariant(status: TestRun["status"]) {
  switch (status) {
    case "completed":
      return "success"
    case "running":
      return "info"
    case "failed":
      return "danger"
    default:
      return "warning"
  }
}

async function refresh() {
  await snapshotStore.refreshRuns()
}

async function startRun() {
  if (!run.value) return
  actionLoading.value = "start"
  try {
    await snapshotStore.startTestRun(run.value.id)
    toastStore.success("Run start requested")
    await snapshotStore.refreshRuns()
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    actionLoading.value = null
  }
}

async function stopRun() {
  if (!run.value) return
  actionLoading.value = "stop"
  try {
    await snapshotStore.stopTestRun(run.value.id)
    toastStore.success("Stop request sent")
    await snapshotStore.refreshRuns()
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    actionLoading.value = null
  }
}

async function repeatRun() {
  if (!run.value) return
  actionLoading.value = "repeat"
  try {
    const clone = await snapshotStore.repeatRun(run.value.id)
    toastStore.success("Test run cloned")
    await snapshotStore.refreshRuns()
    router.push({ name: "testRuns.detail", params: { runId: clone.id } })
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    actionLoading.value = null
  }
}

</script>
