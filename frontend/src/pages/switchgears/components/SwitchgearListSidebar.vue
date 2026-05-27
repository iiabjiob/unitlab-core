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
  <div class="switchgear-list-sidebar">
    <div class="switchgear-list-sidebar__header">
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
        class="switchgear-list-sidebar__workspace-hint"
      >
        Choose a workspace to start configuring
      </p>
    </div>

    <div class="switchgear-list-sidebar__search">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="switchgear-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search switchgears…'"
        class="switchgear-list-sidebar__search-input"
      />
    </div>

    <div class="switchgear-list-sidebar__list">
      <div
        v-if="workspaceMissing"
        class="switchgear-list-sidebar__empty"
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
            <div class="switchgear-list-sidebar__empty">
              No switchgears found
            </div>
          </template>
        </UiSidebarListbox>
      </template>
    </div>

  </div>
</template>

<style scoped>
.switchgear-list-sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
}

.switchgear-list-sidebar__header,
.switchgear-list-sidebar__search {
  margin-bottom: 0.75rem;
}

.switchgear-list-sidebar__workspace-hint {
  margin: 0.5rem 0 0;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-list-sidebar__search-input {
  width: 100%;
  padding: 0.5rem 0.75rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: 0.5rem;
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  outline: none;
}

.switchgear-list-sidebar__search-input::placeholder {
  color: var(--color-neutral-500);
}

.switchgear-list-sidebar__search-input:disabled {
  opacity: 0.6;
}

.switchgear-list-sidebar__list {
  flex: 1 1 auto;
  overflow-y: auto;
}

.switchgear-list-sidebar__list > :deep(* + *) {
  margin-top: 0.25rem;
}

.switchgear-list-sidebar__empty {
  padding: 1.5rem 1rem;
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: 1rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  text-align: center;
}

:global(.dark .switchgear-list-sidebar__workspace-hint),
:global(.dark .switchgear-list-sidebar__empty) {
  color: var(--color-neutral-400);
}

:global(.dark .switchgear-list-sidebar__search-input) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-list-sidebar__empty) {
  border-color: var(--color-neutral-700);
}
</style>
