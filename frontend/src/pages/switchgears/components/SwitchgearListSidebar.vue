<script setup lang="ts">
import { ref, computed } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useRouter, useRoute } from "vue-router"
import SwitchgearListItem from "./SwitchgearListItem.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useProjectStore } from "@/stores/projectStore"

const store = useSwitchgearStore()
const router = useRouter()
const route = useRoute()
const projectStore = useProjectStore()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSwitchgear(id: number) {
  router.push({ name: "switchgears.detail", params: { id } })
}

async function addSwitchgear() {
  if (!projectStore.activeProjectId) return
  const created = await store.createAuto()
  openSwitchgear(created.id)
}

// SEARCH
const query = ref("")
const projectMissing = computed(() => !projectStore.activeProjectId)

const filteredSwitchgears = computed(() => {
  if (projectMissing.value) return []
  if (!query.value.trim()) return store.switchgears

  const q = query.value.toLowerCase()

  return store.switchgears.filter(s =>
    s.name.toLowerCase().includes(q) ||
    s.switchgear_type.toLowerCase().includes(q)
  )
})
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <div class="mb-3">
      <UiButton
        variant="primary"
        size="sm"
        full
        :disabled="projectMissing"
        @click="addSwitchgear"
      >
        + New Switchgear
      </UiButton>
      <p
        v-if="projectMissing"
        class="mt-2 text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Choose a project to start configuring
      </p>
    </div>

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="switchgear-search"
        :disabled="projectMissing"
        :placeholder="projectMissing ? 'Select a project to get started' : 'Search switchgears…'"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-if="projectMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Switchgears belong to a project. Pick one to view its presets.
      </div>

      <template v-else>
      <div
        v-for="switchgear in filteredSwitchgears"
        :key="switchgear.id"
      >
        <SwitchgearListItem
          :switchgear="switchgear"
          :active="isActive(switchgear.id)"
          @select="openSwitchgear(switchgear.id)"
        />
      </div>

      <div
        v-if="filteredSwitchgears.length === 0"
        class="text-gray-500 text-xs italic px-2 py-2"
      >
        No switchgears found
      </div>
      </template>
    </div>

  </div>
</template>
