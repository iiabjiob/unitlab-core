<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <header class="flex flex-col gap-1">
      <p class="text-xs font-semibold uppercase tracking-[0.3em] text-neutral-400">Test Runs</p>
      <h1 class="text-2xl font-semibold text-neutral-900 dark:text-neutral-50">Create a new test run</h1>
      <p class="text-sm text-neutral-500 dark:text-neutral-300">
        Combine instructions, map channels to live signals, and we will capture an immutable snapshot the moment you dispatch the run.
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
        <header class="border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
          <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Run definition</h2>
          <p class="text-sm text-neutral-500 dark:text-neutral-400">Instructions plus channel-to-signal bindings determine what gets executed.</p>
        </header>

        <div class="grid gap-4 border-b border-neutral-100 px-5 py-4 dark:border-neutral-800 md:grid-cols-2">
          <div class="flex flex-col gap-2">
            <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Instructions</label>
            <select v-model="builder.sequenceIds" class="input h-32" multiple>
              <option v-for="sequence in sequenceOptions" :key="sequence.id" :value="sequence.id">
                {{ sequenceLabel(sequence) }}
              </option>
            </select>
            <p class="text-xs text-neutral-500">Hold Cmd/Ctrl to select multiple instructions.</p>
          </div>

          <div class="flex flex-col gap-2">
            <label class="text-xs font-semibold uppercase tracking-wide text-neutral-500">Preload allocation (optional)</label>
            <div class="flex gap-3">
              <button
                class="btn-secondary flex-1"
                type="button"
                :disabled="allocationLoading || !hasAllocatedSignals"
                @click="loadAllocationFromLiveSheet"
              >
                <span v-if="allocationLoading">Loading allocation…</span>
                <span v-else>Load live signal allocations</span>
              </button>
            </div>
            <p class="text-xs text-neutral-500">
              Loads current live `signal -> channel` bindings from Signals page. Test run still captures immutable snapshot on dispatch.
            </p>
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
              <input v-model="builder.allowEmptyAllocation" type="checkbox" class="accent-neutral-900" /> Allow empty allocation (channel-only dry run)
            </label>
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
                    <select v-model="entry.signalId" class="input">
                      <option :value="null">Unassigned</option>
                      <option v-for="signal in signalOptions" :key="signal.id" :value="signal.id">
                        {{ signalLabel(signal) }}
                      </option>
                    </select>
                  </td>
                  <td class="px-3 py-2">
                    <textarea v-model="entry.metadataText" class="input" rows="2" placeholder='{ "wire": "A1" }'></textarea>
                  </td>
                  <td class="px-3 py-2 text-right">
                    <button class="btn-tertiary" type="button" :disabled="builder.entries.length === 1" @click="removeEntry(index)">Remove</button>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          <div class="mt-4 flex flex-wrap items-center justify-end gap-3">
            <span v-if="!allocationReady && !builder.allowEmptyAllocation" class="text-xs text-red-500">Provide at least one channel binding or enable empty allocation.</span>
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

import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSignalsStore } from "@/stores/signalStore"
import { useTestRunStore } from "@/stores/testRunStore"
import { useToastStore } from "@/stores/toastStore"
import type { AllocationEntryInput, Signal } from "@/types/signal"
import type { SequenceDef } from "@/types/sequences"

interface AllocationEntryDraft {
  channelId: number | null
  signalId: number | null
  metadataText: string
}

const signalSheetStore = useSignalSheetStore()
const sequenceStore = useSequenceStore()
const workspaceStore = useWorkspaceStore()
const signalsStore = useSignalsStore()
const testRunStore = useTestRunStore()
const toastStore = useToastStore()
const router = useRouter()

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const sequenceOptions = computed(() => sequenceStore.sequences)
const signalOptions = computed(() => signalsStore.activeSignals)
const signalById = computed(() => {
  const map = new Map<number, Signal>()
  signalsStore.signals.forEach(signal => map.set(signal.id, signal))
  return map
})
const hasAllocatedSignals = computed(() =>
  signalSheetStore.allocationRows.some(row => Number.isFinite(row.channel_id as number)),
)

const builder = reactive({
  sequenceIds: [] as number[],
  notes: "",
  allowEmptyAllocation: false,
  entries: [createEntry()],
})

const submitLoading = ref(false)
const allocationLoading = ref(false)

function createEntry(): AllocationEntryDraft {
  return { channelId: null, signalId: null, metadataText: "" }
}

function resetBuilder() {
  builder.sequenceIds = []
  builder.notes = ""
  builder.allowEmptyAllocation = false
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
      builder.notes = ""
      builder.allowEmptyAllocation = false
      return
    }
    await initializeData()
  },
)

async function initializeData() {
  await Promise.allSettled([
    signalSheetStore.bootstrap(true),
    testRunStore.refreshRuns(),
    sequenceStore.ensureLoaded(),
    signalsStore.refreshSignals(true),
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

async function createRun() {
  if (!canSubmit.value) return
  submitLoading.value = true
  try {
    const allocationEntries = parseEntries()
    const payload = {
      sequence_ids: [...builder.sequenceIds],
      allocation: allocationEntries.length || builder.notes
        ? {
            notes: builder.notes || null,
            entries: allocationEntries,
          }
        : undefined,
      allow_empty_allocation: builder.allowEmptyAllocation || allocationEntries.length === 0,
    }
    const run = await testRunStore.createTestRun(payload)
    toastStore.success("Test run created")
    await testRunStore.refreshRuns(true)
    resetBuilder()
    router.push({ name: "testRuns.detail", params: { runId: run.id } })
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    submitLoading.value = false
  }
}

async function loadAllocationFromLiveSheet() {
  allocationLoading.value = true
  try {
    await signalSheetStore.refreshAllocations()
    const rows = signalSheetStore.allocationRows.filter(row => Number.isFinite(row.channel_id as number))
    if (!rows.length) {
      toastStore.info("No allocated signals in active signal sheet")
      return
    }
    builder.entries = rows.map((row) => {
      const match = signalById.value.get(row.signal_id)
      return {
        channelId: Number(row.channel_id) || null,
        signalId: match?.id ?? null,
        metadataText: JSON.stringify(
          {
            signal_key: row.signal_key,
            signal_name: row.signal_name,
            signal_direction: row.signal_direction,
            source: "live_signal_sheet",
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

function sequenceLabel(sequence: SequenceDef) {
  return `${sequence.name} (#${sequence.id})`
}

function signalLabel(signal: Signal) {
  return `${signal.name} · ${signal.key}`
}
</script>
