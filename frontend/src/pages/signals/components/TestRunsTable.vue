<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <section class="rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
      <header class="border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
        <div>
          <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Test Run Builder</h2>
          <p class="text-sm text-neutral-500 dark:text-neutral-400">We capture a fresh live-signal snapshot when you dispatch the run.</p>
        </div>
      </header>

      <div class="grid gap-4 border-b border-neutral-100 px-5 py-4 dark:border-neutral-800 md:grid-cols-2">
        <div class="flex flex-col gap-2">
          <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Instructions</label>
          <select v-model="builder.sequenceIds" class="input h-32" multiple>
            <option
              v-for="sequence in sequenceOptions"
              :key="sequence.id"
              :value="sequence.id"
            >
              {{ sequenceLabel(sequence) }}
            </option>
          </select>
          <p class="text-xs text-neutral-500">Hold Cmd/Ctrl to select multiple instructions.</p>
        </div>

        <div class="flex flex-col gap-2">
          <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Preload allocation (optional)</label>
          <select v-model.number="allocationSnapshotId" class="input">
            <option :value="null">Select locked snapshot</option>
            <option v-for="snapshot in lockedSnapshots" :key="snapshot.id" :value="snapshot.id">
              {{ snapshot.source_filename ?? `Snapshot #${snapshot.id}` }}
            </option>
          </select>
          <button
            class="btn-secondary"
            :disabled="!allocationSnapshotId || allocationLoading"
            @click="loadAllocationFromSnapshot"
          >
            <span v-if="allocationLoading">Loading allocation...</span>
            <span v-else>Load allocation from snapshot</span>
          </button>
          <p class="text-xs text-neutral-500">Useful for migrating legacy spreadsheets; we still freeze live signals per run.</p>
        </div>

        <div class="md:col-span-2 flex flex-col gap-2">
          <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Notes</label>
          <textarea
            v-model="builder.notes"
            class="input"
            rows="2"
            placeholder="Optional operator notes for this run"
          ></textarea>
          <label class="flex items-center gap-2 text-xs font-semibold uppercase tracking-wide text-neutral-500">
            <input v-model="builder.allowEmptyAllocation" type="checkbox" class="accent-neutral-900" /> Allow empty allocation (channel-only)
          </label>
        </div>
      </div>

      <div class="px-5 py-4">
        <div class="flex items-center justify-between">
          <div>
            <p class="text-sm font-semibold text-neutral-800 dark:text-neutral-100">Allocation entries</p>
            <p class="text-xs text-neutral-500">Each channel used by the selected instructions must appear below.</p>
          </div>
          <button class="btn-secondary" @click="addEntry">Add entry</button>
        </div>

        <div class="mt-3 overflow-x-auto">
          <table class="min-w-full divide-y divide-neutral-200 text-sm dark:divide-neutral-800">
            <thead>
              <tr>
                <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Channel ID</th>
                <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Signal</th>
                <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Metadata (JSON)</th>
                <th class="px-3 py-2"></th>
              </tr>
            </thead>
            <tbody class="divide-y divide-neutral-100 dark:divide-neutral-800">
              <tr v-for="(entry, index) in builder.entries" :key="index">
                <td class="px-3 py-2">
                  <input v-model.number="entry.channelId" type="number" class="input w-32" placeholder="ID" />
                </td>
                <td class="px-3 py-2">
                  <select v-model.number="entry.signalId" class="input">
                    <option :value="null">Unassigned</option>
                    <option v-for="signal in signalOptions" :key="signal.id" :value="signal.id">{{ signalLabel(signal) }}</option>
                  </select>
                </td>
                <td class="px-3 py-2">
                  <textarea v-model="entry.metadataText" class="input" rows="2" placeholder='{ "sheet": "Main" }'></textarea>
                </td>
                <td class="px-3 py-2 text-right">
                  <button class="btn-tertiary" :disabled="builder.entries.length === 1" @click="removeEntry(index)">Remove</button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>

        <div class="mt-4 flex items-center justify-end gap-2">
          <span v-if="!allocationReady && !builder.allowEmptyAllocation" class="text-xs text-red-500">Provide at least one channel binding or enable empty allocation.</span>
          <button class="btn-primary" :disabled="!canSubmit || submitLoading" @click="run">
            <span v-if="submitLoading">Starting...</span>
            <span v-else>Start test run</span>
          </button>
        </div>
      </div>
    </section>

    <section class="flex-1 min-h-0 overflow-hidden rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
      <header class="flex items-center justify-between border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
        <div>
          <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Run History</h2>
          <p class="text-sm text-neutral-500">Latest runs for this workspace.</p>
        </div>
        <button class="btn-secondary" :disabled="runsLoading" @click="testRunStore.refreshRuns(true)">
          <span v-if="runsLoading">Refreshing...</span>
          <span v-else>Refresh</span>
        </button>
      </header>

      <div class="flex-1 overflow-y-auto">
        <table class="min-w-full divide-y divide-neutral-200 text-sm dark:divide-neutral-800">
          <thead>
            <tr>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Run</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Rev</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Snapshot</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Instructions</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Bindings</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Status</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Created</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Snapshot captured</th>
              <th class="px-3 py-2 text-right text-xs font-semibold uppercase tracking-wide text-neutral-500">Actions</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-neutral-100 dark:divide-neutral-800" v-if="runs.length">
            <tr v-for="run in runs" :key="run.id">
              <td class="px-3 py-2 font-medium text-neutral-900 dark:text-neutral-50">#{{ run.id }}</td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">r{{ run.allocation_revision ?? 1 }}</td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">
                <span class="text-neutral-400" v-if="!run.snapshot">Pending</span>
                <span v-else>#{{ run.snapshot.test_run_id }}</span>
              </td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">{{ describeSequences(run) }}</td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">{{ run.allocation?.entries.length ?? 0 }}</td>
              <td class="px-3 py-2">
                <UiBadge :variant="statusVariant(run.status)">{{ run.status }}</UiBadge>
              </td>
              <td class="px-3 py-2 text-neutral-500">{{ formatDate(run.created_at) }}</td>
              <td class="px-3 py-2 text-neutral-500">{{ run.snapshot ? formatDate(run.snapshot.captured_at) : "—" }}</td>
              <td class="px-3 py-2 text-right">
                <div class="flex justify-end gap-2">
                  <button class="btn-tertiary" :disabled="preflightLoading[run.id]" @click="preflight(run.id)">
                    {{ preflightLoading[run.id] ? "Preflight…" : "Preflight" }}
                  </button>
                  <button class="btn-tertiary" :disabled="journalLoading[run.id]" @click="exportJournal(run.id)">
                    {{ journalLoading[run.id] ? "Exporting…" : "Cable" }}
                  </button>
                  <button class="btn-secondary" :disabled="actionLoading === run.id" @click="start(run.id)">Start</button>
                  <button class="btn-tertiary" :disabled="actionLoading === run.id" @click="stop(run.id)">Stop</button>
                  <button class="btn-tertiary" @click="repeat(run.id)">Clone</button>
                </div>
              </td>
            </tr>
          </tbody>
          <tbody v-else>
            <tr>
              <td colspan="9" class="px-3 py-10 text-center text-sm text-neutral-500">No runs recorded yet.</td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from "vue"

import UiBadge from "@/components/ui/UiBadge.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useSignalsStore } from "@/stores/signalStore"
import { useTestRunStore } from "@/stores/testRunStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import type { AllocationEntryInput, Signal, TestRunRecord } from "@/types/signal"
import type { SequenceDef } from "@/types/sequences"
import { formatDate } from "@/utils/datetime"

interface AllocationEntryDraft {
  channelId: number | null
  signalId: number | null
  metadataText: string
}

const snapshotStore = useSignalSnapshotStore()
const signalsStore = useSignalsStore()
const testRunStore = useTestRunStore()
const sequenceStore = useSequenceStore()
const toastStore = useToastStore()

const runs = computed(() => testRunStore.testRuns)
const runsLoading = computed(() => testRunStore.loading)
const lockedSnapshots = computed(() => snapshotStore.lockedSnapshots)
const sequenceOptions = computed(() => sequenceStore.sequences)
const signalOptions = computed(() => signalsStore.activeSignals)
const sequenceNameMap = computed(() => {
  const map = new Map<number, SequenceDef>()
  sequenceStore.sequences.forEach(seq => map.set(seq.id, seq))
  return map
})
const signalByKey = computed(() => {
  const map = new Map<string, Signal>()
  signalsStore.signals.forEach(signal => map.set(signal.key, signal))
  return map
})

const builder = reactive({
  sequenceIds: [] as number[],
  notes: "",
  allowEmptyAllocation: false,
  entries: [createEntry()],
})

const allocationSnapshotId = ref<number | null>(null)

const submitLoading = ref(false)
const allocationLoading = ref(false)
const actionLoading = ref<number | null>(null)
const preflightLoading = reactive<Record<number, boolean>>({})
const journalLoading = reactive<Record<number, boolean>>({})

onMounted(() => {
  void Promise.allSettled([
    testRunStore.refreshRuns(),
    sequenceStore.ensureLoaded(),
    signalsStore.refreshSignals(true),
    snapshotStore.refreshSnapshots(),
  ])
})

watch(
  () => lockedSnapshots.value,
  (snapshots) => {
    if (!allocationSnapshotId.value && snapshots.length) {
      allocationSnapshotId.value = snapshots[0].id
    }
  },
  { immediate: true },
)

function createEntry(): AllocationEntryDraft {
  return { channelId: null, signalId: null, metadataText: "" }
}

function addEntry() {
  builder.entries.push(createEntry())
}

function removeEntry(index: number) {
  if (builder.entries.length === 1) return
  builder.entries.splice(index, 1)
}

const allocationReady = computed(() => builder.entries.some(entry => Number.isFinite(entry.channelId)))

const canSubmit = computed(() => {
  if (!builder.sequenceIds.length) return false
  if (!builder.allowEmptyAllocation && !allocationReady.value) return false
  return true
})

function parseEntries(): AllocationEntryInput[] {
  const entries: AllocationEntryInput[] = []
  builder.entries.forEach((entry, index) => {
    if (!Number.isFinite(entry.channelId)) {
      return
    }
    let metadata: Record<string, unknown> | null = null
    if (entry.metadataText.trim()) {
      try {
        metadata = JSON.parse(entry.metadataText)
      } catch (err) {
        throw new Error(`Allocation row ${index + 1}: invalid metadata JSON`)
      }
    }
    entries.push({
      channel_id: entry.channelId as number,
      signal_id: Number.isFinite(entry.signalId) ? (entry.signalId as number) : null,
      signal_metadata: metadata,
    })
  })
  return entries
}

async function run() {
  if (!canSubmit.value) return
  submitLoading.value = true
  try {
    const allocation = parseEntries()
    const payload = {
      sequence_ids: [...builder.sequenceIds],
      allocation: allocation.length || builder.notes
        ? {
            notes: builder.notes || null,
            entries: allocation,
          }
        : undefined,
      allow_empty_allocation: builder.allowEmptyAllocation || allocation.length === 0,
    }
    const run = await testRunStore.createTestRun(payload)
    const check = await preflight(run.id)
    if (check?.ready) {
      await testRunStore.startTestRun(run.id)
      toastStore.success("Test run dispatched")
    } else {
      toastStore.info(`Run #${run.id} created, preflight requires reallocation`)
    }
    builder.sequenceIds = []
    builder.entries = [createEntry()]
    builder.notes = ""
    builder.allowEmptyAllocation = false
    await testRunStore.refreshRuns(true)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    submitLoading.value = false
  }
}

async function loadAllocationFromSnapshot() {
  if (!allocationSnapshotId.value) return
  allocationLoading.value = true
  try {
    const allocation = await snapshotStore.getAllocation(allocationSnapshotId.value, true)
    if (!allocation.mapping.length) {
      toastStore.info("Snapshot allocation is empty")
      return
    }
    builder.entries = allocation.mapping.map((item) => {
      const match = item.signal_key ? signalByKey.value.get(item.signal_key) : null
      return {
        channelId: Number(item.channel_id) || null,
        signalId: match?.id ?? null,
        metadataText: JSON.stringify(
          {
            snapshot_signal_key: item.signal_key ?? null,
            snapshot_row_index: item.signal_row_index,
            snapshot_meta: item.meta ?? null,
          },
          null,
          2,
        ),
      }
    })
    if (!builder.entries.length) {
      builder.entries = [createEntry()]
    }
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    allocationLoading.value = false
  }
}

async function repeat(runId: number) {
  try {
    await testRunStore.repeatTestRun(runId)
    toastStore.success("Test run cloned")
    await testRunStore.refreshRuns(true)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function preflight(runId: number) {
  preflightLoading[runId] = true
  try {
    const result = await testRunStore.preflightTestRun(runId)
    if (result.ready) {
      toastStore.success(`Run #${runId} preflight passed`)
    } else {
      toastStore.info(`Run #${runId} requires reallocation before start`)
    }
    return result
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
    return null
  } finally {
    preflightLoading[runId] = false
  }
}

async function start(runId: number) {
  actionLoading.value = runId
  try {
    const check = await preflight(runId)
    if (!check || !check.ready) {
      return
    }
    await testRunStore.startTestRun(runId)
    toastStore.success(`Run #${runId} started`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    actionLoading.value = null
  }
}

async function stop(runId: number) {
  actionLoading.value = runId
  try {
    await testRunStore.stopTestRun(runId)
    toastStore.success(`Run #${runId} stop requested`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    actionLoading.value = null
  }
}

async function exportJournal(runId: number) {
  journalLoading[runId] = true
  try {
    const csv = await testRunStore.exportCableJournal(runId)
    const blob = new Blob([csv], { type: "text/csv;charset=utf-8;" })
    const url = URL.createObjectURL(blob)
    const link = document.createElement("a")
    link.href = url
    link.download = `test-run-${runId}-cable-journal.csv`
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    URL.revokeObjectURL(url)
    toastStore.success(`Run #${runId} cable journal exported`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    journalLoading[runId] = false
  }
}

function sequenceLabel(sequence: SequenceDef) {
  return `${sequence.name} (#${sequence.id})`
}

function describeSequences(run: TestRunRecord) {
  if (!run.sequence_ids || !run.sequence_ids.length) return "—"
  return run.sequence_ids
    .map(id => sequenceNameMap.value.get(id)?.name || `#${id}`)
    .join(", ")
}

function signalLabel(signal: Signal) {
  return `${signal.name} · ${signal.key}`
}

function statusVariant(status: TestRunRecord["status"]) {
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
</script>
