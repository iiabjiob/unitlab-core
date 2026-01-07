<script setup lang="ts">
import { computed, ref } from "vue"
import { useRouter, useRoute } from "vue-router"
import UiButton from "@/components/ui/UiButton.vue"
import { useProjectStore } from "@/stores/projectStore"
import { useTestRunStore } from "@/stores/testRunStore"
import TestRunListItem from "./TestRunListItem.vue"

const projectStore = useProjectStore()
const runStore = useTestRunStore()
const router = useRouter()
const route = useRoute()

const query = ref("")
const creating = ref(false)

const projectMissing = computed(() => !projectStore.activeProjectId)

const filteredRuns = computed(() => {
  if (projectMissing.value) return []
  if (!query.value.trim()) return runStore.runs
  const q = query.value.toLowerCase()
  return runStore.runs.filter(run => run.name.toLowerCase().includes(q))
})

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openRun(id: number) {
  router.push(`/tests/${id}`)
}

function addRun() {
  if (!projectStore.activeProjectId) return
  creating.value = true
  runStore
    .createTestRun({ name: "", channel_ids: [] })
    .then(run => {
      router.push(`/tests/${run.id}`)
    })
    .finally(() => {
      creating.value = false
    })
}
</script>

<template>
  <div class="h-full flex flex-col">
    <div class="mb-3">
      <UiButton
        variant="primary"
        size="sm"
        full
        :disabled="projectMissing || creating"
        @click="addRun"
      >
        {{ creating ? "Creating…" : "+ New Test Run" }}
      </UiButton>
      <p
        v-if="projectMissing"
        class="mt-2 text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Use the project switcher to enable edits
      </p>
    </div>

    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="test-search"
        :disabled="projectMissing"
        :placeholder="projectMissing ? 'Select a project to get started' : 'Search tests…'"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-if="projectMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Select or create a project to see its tests.
      </div>

      <template v-else>
        <div
          v-for="run in filteredRuns"
          :key="run.id"
        >
          <TestRunListItem
            :run="run"
            :active="isActive(run.id)"
            @select="openRun(run.id)"
          />
        </div>

        <div
          v-if="filteredRuns.length === 0"
          class="text-gray-500 text-xs italic px-2 py-2"
        >
          No tests found
        </div>
      </template>
    </div>
  </div>
</template>
