<script setup lang="ts">
import { ref, computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useRouter, useRoute } from "vue-router"
import SequenceListItem from "./SequenceListItem.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"

const store = useSequenceStore()
const router = useRouter()
const route = useRoute()
const workspaceStore = useWorkspaceStore()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSequence(id: number) {
  router.push(`/test-runs/instructions/${id}`)
}

function addSequence() {
  if (!workspaceStore.activeWorkspaceId) return
  store.createSequenceAuto().then(seq => {
    router.push(`/test-runs/instructions/${seq.id}`)
  })
}

// SEARCH
const query = ref("")
const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const filteredSequences = computed(() => {
  if (workspaceMissing.value) return []
  if (!query.value.trim()) return store.sequences

  const q = query.value.toLowerCase()

  return store.sequences.filter(s =>
    s.name.toLowerCase().includes(q) ||
    (s.description && s.description.toLowerCase().includes(q))
  )
})

const selectedId = computed<number | null>(() => {
  const parsed = Number(route.params.id)
  return Number.isFinite(parsed) ? parsed : null
})

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  openSequence(parsed)
}
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <div class="mb-3">
      <UiButton
        variant="primary"
        size="sm"
        full
        :disabled="workspaceMissing"
        @click="addSequence"
      >
        + New Instruction
      </UiButton>
      <p
        v-if="workspaceMissing"
        class="mt-2 text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Use the workspace switcher to enable edits
      </p>
    </div>

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="instruction-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search instructions…'"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-if="workspaceMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Select or create a workspace to see its instructions.
      </div>

      <template v-else>
      <UiSidebarListbox
        :items="filteredSequences"
        :active-id="selectedId"
        aria-label="Instructions"
        @select="handleSelect"
      >
        <template #item="{ item: seq, isCursor }">
          <SequenceListItem
            :sequence="seq"
            :active="isActive(seq.id) || isCursor"
          />
        </template>
        <template #empty>
          <div class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
            No instructions found
          </div>
        </template>
      </UiSidebarListbox>
      </template>
    </div>

  </div>
</template>
