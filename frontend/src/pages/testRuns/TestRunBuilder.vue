<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <header class="flex flex-col gap-1">
      <p class="text-xs font-semibold uppercase tracking-[0.3em] text-neutral-400">Test Runs</p>
      <h1 class="text-2xl font-semibold text-neutral-900 dark:text-neutral-50">Create a new test run</h1>
      <p class="text-sm text-neutral-500 dark:text-neutral-300">
        Combine sequences, choose execution mode, and attach allocation bindings. Runs execute only after you dispatch them from the detail page.
      </p>
    </header>

    <div
      v-if="workspaceMissing"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 px-6 py-10 text-center text-sm text-neutral-500 dark:border-neutral-700 dark:text-neutral-300"
    >
      Select a workspace to build test runs.
    </div>

    <section
      v-else
      class="flex-1 overflow-y-auto rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900"
    >
      <form class="flex flex-col" @submit.prevent="createRun">
        <header class="flex items-center justify-between border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
          <div>
            <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Run definition</h2>
            <p class="text-sm text-neutral-500 dark:text-neutral-400">Choose how the run should execute.</p>
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
                type="button"
                :disabled="!builder.snapshotId || allocationLoading"
                @click="loadAllocationFromSnapshot"
              >
                <span v-if="allocationLoading">Loading allocation…</span>
                <span v-else>Load allocation from snapshot</span>
              </button>
            </div>
          </div>

          <div class="flex flex-col gap-2">
            <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Sequences</label>
            <select v-model="builder.sequenceIds" class="input h-32" multiple>
              <option v-for="sequence in sequenceOptions" :key="sequence.id" :value="sequence.id">
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

        <div class="px-5 py-5">
          <div class="flex items-center justify-between">
            <div>
              <p class="text-sm font-semibold text-neutral-800 dark:text-neutral-100">Allocation entries</p>
              <p class="text-xs text-neutral-500">Each physical channel you intend to use must be listed.</p>
            </div>
            <button class="btn-secondary" type="button" @click="addEntry">Add entry</button>
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
                    <button class="btn-tertiary" type="button" :disabled="builder.entries.length === 1" @click="removeEntry(index)">Remove</button>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          <div class="mt-4 flex flex-wrap items-center justify-end gap-3">
            <span v-if="!allocationReady" class="text-xs text-red-500">Provide at least one channel binding.</span>
            <button class="btn-primary" type="submit" :disabled="!canSubmit || submitLoading">
              <span v-if="submitLoading">Creating…</span>
              <span v-else>Create test run</span>
            </button>
          </div>
        </div>
      </form>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref, watch } from "vue"
import { useRouter } from "vue-router"

import UiBadge from "@/components/ui/UiBadge.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"
import type { TestRunAllocationEntryInput, TestRunMode } from "@/types/signal"
import type { SequenceDef } from "@/types/sequences"

interface AllocationEntryDraft {
  channelId: number | null
  signalKey: string
  metadataText: string
}

const snapshotStore = useSignalSnapshotStore()
const sequenceStore = useSequenceStore()
const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()
const router = useRouter()

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const lockedSnapshots = computed(() => snapshotStore.lockedSnapshots)
const sequenceOptions = computed(() => sequenceStore.sequences)

const builder = reactive({
  mode: "signal" as TestRunMode,
  snapshotId: null as number | null,
  sequenceIds: [] as number[],
  notes: "",
  entries: [createEntry()],
})

const submitLoading = ref(false)
const allocationLoading = ref(false)

function createEntry(): AllocationEntryDraft {
  return { channelId: null, signalKey: "", metadataText: "" }
}

function resetBuilder() {
  builder.mode = "signal"
  builder.snapshotId = lockedSnapshots.value[0]?.id ?? null
  builder.sequenceIds = []
  builder.notes = ""
  builder.entries = [createEntry()]
}

onMounted(() => {
  if (!workspaceMissing.value) {
    void initializeData()
  }
})

watch(
  () => workspaceStore.activeWorkspaceId,
  async () => {
    if (workspaceMissing.value) {
      builder.entries = [createEntry()]
      builder.sequenceIds = []
      builder.snapshotId = null
      builder.notes = ""
      return
    }
    await initializeData()
  },
)

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
)

async function initializeData() {
  await Promise.allSettled([
    snapshotStore.refreshSnapshots(),
    snapshotStore.refreshRuns(),
    sequenceStore.ensureLoaded(),
  ])
  resetBuilder()
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
  if (workspaceMissing.value) return false
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

async function createRun() {
  if (!canSubmit.value) return
  submitLoading.value = true
  try {
    const allocationEntries = parseEntries()
    const payload = {
      sequence_ids: [...builder.sequenceIds],
      mode: builder.mode,
      signal_snapshot_id: builder.mode === "signal" ? builder.snapshotId : null,
      allocation: {
        notes: builder.notes || null,
        entries: allocationEntries,
      },
    }
    const run = await snapshotStore.createTestRun(payload)
    toastStore.success("Test run created")
    await snapshotStore.refreshRuns()
    resetBuilder()
    router.push({ name: "testRuns.detail", params: { runId: run.id } })
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
    builder.entries = allocation.mapping.map(item => ({
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

function sequenceLabel(sequence: SequenceDef) {
  return `${sequence.name} (#${sequence.id})`
}
</script>
