<template>
  <div class="flex h-full flex-col">
    <div class="mb-3 space-y-2">
      <div class="flex items-center gap-2">
        <UiButton variant="primary" size="sm" full :disabled="workspaceMissing" @click="emit('create')">
          + New Test Run
        </UiButton>
        <UiMenu v-if="selectedRun">
          <UiMenuTrigger asChild>
            <UiButton
              variant="icon"
              aria-label="Run actions"
              @click.stop
              @pointerdown.stop
            >
              <EllipsisHorizontalIcon size="20" />
            </UiButton>
          </UiMenuTrigger>
          <UiMenuContent>
            <UiMenuItem class="text-neutral-900 dark:text-neutral-100" @select="repeatSelectedRun">
              Repeat
            </UiMenuItem>
            <UiMenuItem
              v-if="selectedRun.status !== 'running'"
              class="text-neutral-900 dark:text-neutral-100"
              @select="startSelectedRun"
            >
              Start run
            </UiMenuItem>
            <UiMenuItem
              v-else
              class="text-neutral-900 dark:text-neutral-100"
              @select="stopSelectedRun"
            >
              Stop run
            </UiMenuItem>
          </UiMenuContent>
        </UiMenu>
      </div>
      <p
        v-if="workspaceMissing"
        class="text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Select a workspace to manage test runs
      </p>
    </div>

    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="test-run-search"
        :disabled="workspaceMissing"
        placeholder="Search test runs…"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <div class="flex-1 space-y-2 overflow-y-auto">
      <div
        v-if="workspaceMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Test runs belong to a workspace. Pick one to continue.
      </div>

      <div
        v-else-if="loading"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Loading test runs…
      </div>

      <template v-else>
        <UiSidebarListbox
          :items="filteredRuns"
          :active-id="selectedId"
          aria-label="Test runs"
          @select="handleSelect"
        >
          <template #item="{ item: run, isCursor }">
            <TestRunListItem
              :run="run"
              :active="run.id === selectedId || isCursor"
              :sequence-name-map="sequenceNameMap"
            />
          </template>
          <template #empty>
            <div class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
              No test runs found
            </div>
          </template>
        </UiSidebarListbox>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { useRouter } from "vue-router"

import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import TestRunListItem from "./TestRunListItem.vue"
import type { TestRunRecord } from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useTestRunStore } from "@/stores/testRunStore"
import { useToastStore } from "@/stores/toastStore"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

const props = defineProps<{
  runs: TestRunRecord[]
  selectedId: number | null
  loading?: boolean
  sequenceNameMap: Map<number, string>
}>()

const emit = defineEmits<{ (e: "create"): void; (e: "select", id: number): void }>()

const workspaceStore = useWorkspaceStore()
const testRunStore = useTestRunStore()
const toastStore = useToastStore()
const router = useRouter()
const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const query = ref("")

const loading = computed(() => props.loading ?? false)

const normalizedRuns = computed(() => {
  return [...props.runs].sort((a, b) => {
    return new Date(b.created_at).getTime() - new Date(a.created_at).getTime()
  })
})

const filteredRuns = computed(() => {
  if (workspaceMissing.value) return []
  const q = query.value.trim().toLowerCase()
  if (!q) return normalizedRuns.value
  return normalizedRuns.value.filter(run => {
    const idMatch = `#${run.id}`.includes(q)
    const statusMatch = run.status.toLowerCase().includes(q)
    const notesMatch = (run.allocation?.notes?.toLowerCase() ?? "").includes(q)
    const sequenceMatch = run.sequence_ids.some(id => {
      const name = props.sequenceNameMap.get(id)
      if (!name) return false
      return name.toLowerCase().includes(q)
    })
    return idMatch || statusMatch || notesMatch || sequenceMatch
  })
})

const selectedRun = computed(() => {
  if (props.selectedId === null) return null
  return props.runs.find((run) => run.id === props.selectedId) ?? null
})

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  emit("select", parsed)
}

async function repeatSelectedRun() {
  if (!selectedRun.value) return
  try {
    const clone = await testRunStore.repeatTestRun(selectedRun.value.id)
    await router.push({ name: "testRuns.detail", params: { runId: clone.id } })
    toastStore.success(`Created run #${clone.id}`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function startSelectedRun() {
  if (!selectedRun.value) return
  try {
    await testRunStore.startTestRun(selectedRun.value.id)
    toastStore.success("Run start requested")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function stopSelectedRun() {
  if (!selectedRun.value) return
  try {
    await testRunStore.stopTestRun(selectedRun.value.id)
    toastStore.success("Stop request sent")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}
</script>
