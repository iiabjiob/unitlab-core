<script setup lang="ts">
import { ref, computed } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useRouter, useRoute } from "vue-router"
import SwitchgearListItem from "./SwitchgearListItem.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useToastStore } from "@/stores/toastStore"

const store = useSwitchgearStore()
const router = useRouter()
const route = useRoute()
const workspaceStore = useWorkspaceStore()
const toastStore = useToastStore()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSwitchgear(id: number) {
  router.push({ name: "switchgears.detail", params: { id } })
}

async function addSwitchgear() {
  if (!workspaceStore.activeWorkspaceId) return
  const created = await store.createAuto()
  const assigned = (created.bindings ?? []).filter(binding => Number.isFinite(binding.channel_id as number)).length
  if (assigned < 4) {
    toastStore.warning(`Auto-allocation assigned ${assigned}/4 channels. Complete remaining bindings manually.`)
  }
  openSwitchgear(created.id)
}

// SEARCH
const query = ref("")
const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const filteredSwitchgears = computed(() => {
  if (workspaceMissing.value) return []
  if (!query.value.trim()) return store.switchgears

  const q = query.value.toLowerCase()

  return store.switchgears.filter(s =>
    s.name.toLowerCase().includes(q) ||
    s.switchgear_type.toLowerCase().includes(q)
  )
})

const selectedId = computed<number | null>(() => {
  const parsed = Number(route.params.id)
  return Number.isFinite(parsed) ? parsed : null
})

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  openSwitchgear(parsed)
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
        @click="addSwitchgear"
      >
        + New Switchgear
      </UiButton>
      <p
        v-if="workspaceMissing"
        class="mt-2 text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Choose a workspace to start configuring
      </p>
    </div>

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="switchgear-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search switchgears…'"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-if="workspaceMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Switchgears belong to a workspace. Pick one to view its presets.
      </div>

      <template v-else>
      <UiSidebarListbox
        :items="filteredSwitchgears"
        :active-id="selectedId"
        aria-label="Switchgears"
        @select="handleSelect"
      >
        <template #item="{ item: switchgear, isCursor }">
          <SwitchgearListItem
            :switchgear="switchgear"
            :active="isActive(switchgear.id) || isCursor"
          />
        </template>
        <template #empty>
          <div class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
            No switchgears found
          </div>
        </template>
      </UiSidebarListbox>
      </template>
    </div>

  </div>
</template>
