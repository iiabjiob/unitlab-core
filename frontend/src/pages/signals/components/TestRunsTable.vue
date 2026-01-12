<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <section class="rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
      <header class="flex items-center justify-between border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
        <div>
          <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Test Run Builder</h2>
          <p class="text-sm text-neutral-500 dark:text-neutral-400">Define mode, sequences, and channel bindings before executing.</p>
        </div>
        <UiBadge :variant="builder.mode === 'signal' ? 'info' : 'warning'" class="uppercase">{{ builder.mode }} mode</UiBadge>
      </header>

      <div class="grid gap-4 border-b border-neutral-100 px-5 py-4 dark:border-neutral-800 md:grid-cols-2">
        <div class="flex flex-col gap-2">
          <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Execution mode</label>
          <div class="flex gap-3">
            <label class="flex items-center gap-2 text-sm text-neutral-700 dark:text-neutral-200">
              <input v-model="builder.mode" type="radio" class="accent-neutral-900" value="signal" /> Signal (snapshot)
            </label>
            <label class="flex items-center gap-2 text-sm text-neutral-700 dark:text-neutral-200">
              <input v-model="builder.mode" type="radio" class="accent-neutral-900" value="channel" /> Channel only
            </label>
          </div>
          <p class="text-xs text-neutral-500" v-if="builder.mode === 'channel'">Runs without a signal snapshot. Only channel bindings will be used.</p>
          <div v-else class="flex flex-col gap-2">
            <select v-model.number="builder.snapshotId" class="input">
              <option :value="null">Select locked snapshot</option>
              <option v-for="snapshot in lockedSnapshots" :key="snapshot.id" :value="snapshot.id">
                {{ snapshot.source_filename ?? `Snapshot #${snapshot.id}` }}
              </option>
            </select>
            <button
              class="btn-secondary"
              :disabled="!builder.snapshotId || allocationLoading"
              @click="loadAllocationFromSnapshot"
            >
              <span v-if="allocationLoading">Loading allocation...</span>
              <span v-else>Load allocation from snapshot</span>
            </button>
          </div>
        </div>

        <div class="flex flex-col gap-2">
          <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Sequences</label>
          <select v-model="builder.sequenceIds" class="input h-32" multiple>
            <option
              v-for="sequence in sequenceOptions"
              :key="sequence.id"
              :value="sequence.id"
            >
              {{ sequenceLabel(sequence) }}
            </option>
          </select>
          <p class="text-xs text-neutral-500">Hold Cmd/Ctrl to select multiple sequences.</p>
        </div>

        <div class="md:col-span-2 flex flex-col gap-2">
          <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Notes</label>
          <textarea
            v-model="builder.notes"
            class="input"
            rows="2"
            placeholder="Optional operator notes for this run"
          ></textarea>
        </div>
      </div>

      <div class="px-5 py-4">
        <div class="flex items-center justify-between">
          <div>
            <p class="text-sm font-semibold text-neutral-800 dark:text-neutral-100">Allocation entries</p>
            <p class="text-xs text-neutral-500">Each channel used by the selected sequences must appear below.</p>
          </div>
          <button class="btn-secondary" @click="addEntry">Add entry</button>
        </div>

        <div class="mt-3 overflow-x-auto">
          <table class="min-w-full divide-y divide-neutral-200 text-sm dark:divide-neutral-800">
            <thead>
              <tr>
                <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Channel ID</th>
                <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Signal key</th>
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
                  <input v-model="entry.signalKey" type="text" class="input" placeholder="Optional" />
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
          <span v-if="!allocationReady" class="text-xs text-red-500">Provide at least one channel binding.</span>
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
        <button class="btn-secondary" :disabled="runsLoading" @click="snapshotStore.refreshRuns()">
          <span v-if="runsLoading">Refreshing...</span>
          <span v-else>Refresh</span>
        </button>
      </header>

      <div class="flex-1 overflow-y-auto">
        <table class="min-w-full divide-y divide-neutral-200 text-sm dark:divide-neutral-800">
          <thead>
            <tr>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Run</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Mode</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Snapshot</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Sequences</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Bindings</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Status</th>
              <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Created</th>
              <th class="px-3 py-2 text-right text-xs font-semibold uppercase tracking-wide text-neutral-500">Actions</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-neutral-100 dark:divide-neutral-800" v-if="runs.length">
            <tr v-for="run in runs" :key="run.id">
              <td class="px-3 py-2 font-medium text-neutral-900 dark:text-neutral-50">#{{ run.id }}</td>
              <td class="px-3 py-2">
                <UiBadge :variant="run.mode === 'signal' ? 'info' : 'warning'">{{ run.mode }}</UiBadge>
              </td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">
                <span v-if="run.signal_snapshot_id">Snapshot {{ run.signal_snapshot_id }}</span>
                <span v-else class="text-neutral-400">—</span>
              </td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">{{ describeSequences(run) }}</td>
              <td class="px-3 py-2 text-neutral-700 dark:text-neutral-200">{{ run.allocation?.entries.length ?? 0 }}</td>
              <td class="px-3 py-2">
                <UiBadge :variant="statusVariant(run.status)">{{ run.status }}</UiBadge>
              </td>
              <td class="px-3 py-2 text-neutral-500">{{ formatDate(run.created_at) }}</td>
              <td class="px-3 py-2 text-right">
                <div class="flex justify-end gap-2">
                  <button class="btn-secondary" :disabled="actionLoading === run.id" @click="start(run.id)">Start</button>
                  <button class="btn-tertiary" :disabled="actionLoading === run.id" @click="stop(run.id)">Stop</button>
                  <button class="btn-tertiary" @click="repeat(run.id)">Clone</button>
                </div>
              </td>
            </tr>
          </tbody>
          <tbody v-else>
            <tr>
              <td colspan="8" class="px-3 py-10 text-center text-sm text-neutral-500">No runs recorded yet.</td>
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
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import type { TestRunMode, TestRunAllocationEntryInput } from "@/types/signal"
import type { SequenceDef } from "@/types/sequences"
import { formatDate } from "@/utils/datetime"

interface AllocationEntryDraft {
  channelId: number | null
  signalKey: string
  metadataText: string
}

const snapshotStore = useSignalSnapshotStore()
const sequenceStore = useSequenceStore()
const toastStore = useToastStore()

const runs = computed(() => snapshotStore.runs)
const runsLoading = computed(() => snapshotStore.runsLoading)
const lockedSnapshots = computed(() => snapshotStore.lockedSnapshots)
const sequenceOptions = computed(() => sequenceStore.sequences)
const sequenceNameMap = computed(() => {
  const map = new Map<number, SequenceDef>()
  sequenceStore.sequences.forEach(seq => map.set(seq.id, seq))
  return map
})

const builder = reactive({
  mode: "signal" as TestRunMode,
  snapshotId: null as number | null,
  sequenceIds: [] as number[],
  notes: "",
  entries: [createEntry()],
})

const submitLoading = ref(false)
const allocationLoading = ref(false)
const actionLoading = ref<number | null>(null)

onMounted(() => {
  snapshotStore.refreshRuns()
  sequenceStore.ensureLoaded()
})

watch(
  () => builder.mode,
  (mode) => {
    if (mode === "channel") {
      builder.snapshotId = null
    } else if (!builder.snapshotId && lockedSnapshots.value.length) {
      builder.snapshotId = lockedSnapshots.value[0].id
    }
  },
)

watch(
  () => lockedSnapshots.value,
  (snapshots) => {
    if (builder.mode === "signal" && !builder.snapshotId && snapshots.length) {
      builder.snapshotId = snapshots[0].id
    }
  },
  { immediate: true },
)

function createEntry(): AllocationEntryDraft {
  return { channelId: null, signalKey: "", metadataText: "" }
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
  if (!allocationReady.value) return false
  if (builder.mode === "signal" && !builder.snapshotId) return false
  return true
})

function parseEntries(): TestRunAllocationEntryInput[] {
  const entries: TestRunAllocationEntryInput[] = []
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
      signal_key: entry.signalKey?.trim() || null,
      signal_metadata: metadata,
    })
  })
  if (!entries.length) {
    throw new Error("Provide at least one channel binding")
  }
  return entries
}

async function run() {
  if (!canSubmit.value) return
  submitLoading.value = true
  try {
    const allocation = parseEntries()
    const payload = {
      sequence_ids: [...builder.sequenceIds],
      mode: builder.mode,
      signal_snapshot_id: builder.mode === "signal" ? builder.snapshotId : null,
      allocation: {
        notes: builder.notes || null,
        entries: allocation,
      },
    }
    const run = await snapshotStore.createTestRun(payload)
    await snapshotStore.startTestRun(run.id)
    toastStore.success("Test run dispatched")
    builder.sequenceIds = []
    builder.entries = [createEntry()]
    builder.notes = ""
    await snapshotStore.refreshRuns()
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    submitLoading.value = false
  }
}

async function loadAllocationFromSnapshot() {
  if (!builder.snapshotId) return
  allocationLoading.value = true
  try {
    const allocation = await snapshotStore.getAllocation(builder.snapshotId, true)
    if (!allocation.mapping.length) {
      toastStore.info("Snapshot allocation is empty")
      return
    }
    builder.entries = allocation.mapping.map((item) => ({
      channelId: Number(item.channel_id) || null,
      signalKey: item.signal_key ?? "",
      metadataText: JSON.stringify(
        {
          signal_row_index: item.signal_row_index,
          meta: item.meta ?? null,
        },
        null,
        0,
      ),
    }))
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    allocationLoading.value = false
  }
}

async function repeat(runId: number) {
  try {
    await snapshotStore.repeatRun(runId)
    toastStore.success("Test run cloned")
    await snapshotStore.refreshRuns()
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function start(runId: number) {
  actionLoading.value = runId
  try {
    await snapshotStore.startTestRun(runId)
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
    await snapshotStore.stopTestRun(runId)
    toastStore.success(`Run #${runId} stop requested`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    actionLoading.value = null
  }
}

function sequenceLabel(sequence: SequenceDef) {
  return `${sequence.name} (#${sequence.id})`
}

function describeSequences(run: { sequence_ids: number[] }) {
  if (!run.sequence_ids || !run.sequence_ids.length) return "—"
  return run.sequence_ids
    .map(id => sequenceNameMap.value.get(id)?.name || `#${id}`)
    .join(", ")
}

function statusVariant(status: string) {
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
