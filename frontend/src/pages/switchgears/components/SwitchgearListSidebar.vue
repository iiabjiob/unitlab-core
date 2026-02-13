<script setup lang="ts">
import { ref, computed } from "vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useRouter, useRoute } from "vue-router"
import SwitchgearListItem from "./SwitchgearListItem.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

const store = useSwitchgearStore()
const router = useRouter()
const route = useRoute()
const workspaceStore = useWorkspaceStore()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSwitchgear(id: number) {
  router.push({ name: "switchgears.detail", params: { id } })
}

async function addSwitchgear() {
  if (!workspaceStore.activeWorkspaceId) return
  const created = await store.createAuto()
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

const selectedSwitchgear = computed(() => {
  if (selectedId.value === null) return null
  return store.switchgears.find((item) => item.id === selectedId.value) ?? null
})

const renameOpen = ref(false)
const renameValue = ref("")
const renaming = ref(false)
const deleteOpen = ref(false)

const deleteMessage = computed(() =>
  selectedSwitchgear.value
    ? `Switchgear "${selectedSwitchgear.value.name}" will be deleted.`
    : "",
)

function handleSelect(id: string | number) {
  const parsed = Number(id)
  if (!Number.isFinite(parsed)) return
  openSwitchgear(parsed)
}

function openRenameSelected() {
  if (!selectedSwitchgear.value) return
  renameValue.value = selectedSwitchgear.value.name
  renameOpen.value = true
}

function cancelRenameSelected() {
  renameOpen.value = false
  renameValue.value = selectedSwitchgear.value?.name ?? ""
}

async function confirmRenameSelected() {
  if (!selectedSwitchgear.value) return
  const trimmed = renameValue.value.trim()
  if (!trimmed || trimmed === selectedSwitchgear.value.name) {
    renameOpen.value = false
    return
  }
  renaming.value = true
  try {
    await store.updateField(selectedSwitchgear.value.id, { name: trimmed })
    renameOpen.value = false
  } finally {
    renaming.value = false
  }
}

async function duplicateSelected() {
  if (!selectedSwitchgear.value) return
  const duplicated = await store.duplicate(selectedSwitchgear.value.id)
  await router.push({ name: "switchgears.detail", params: { id: duplicated.id } })
}

async function confirmDeleteSelected() {
  if (!selectedSwitchgear.value) return
  const deletingId = selectedSwitchgear.value.id
  await store.remove(deletingId)
  deleteOpen.value = false
  if (selectedId.value === deletingId) {
    await router.push({ name: "switchgears.list" })
  }
}
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <div class="mb-3">
      <div class="flex items-center gap-2">
        <UiButton
          variant="primary"
          size="sm"
          full
          :disabled="workspaceMissing"
          @click="addSwitchgear"
        >
          + New Switchgear
        </UiButton>
        <UiMenu v-if="selectedSwitchgear">
          <UiMenuTrigger asChild>
            <UiButton
              variant="icon"
              aria-label="Switchgear actions"
              @click.stop
              @pointerdown.stop
            >
              <EllipsisHorizontalIcon size="20" />
            </UiButton>
          </UiMenuTrigger>
          <UiMenuContent>
            <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openRenameSelected">
              Rename
            </UiMenuItem>
            <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="duplicateSelected">
              Duplicate
            </UiMenuItem>
            <UiMenuItem danger @select="deleteOpen = true">
              Delete
            </UiMenuItem>
          </UiMenuContent>
        </UiMenu>
      </div>
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

    <RenameModal
      :open="renameOpen"
      title="Rename switchgear"
      v-model="renameValue"
      :loading="renaming"
      @cancel="cancelRenameSelected"
      @confirm="confirmRenameSelected"
    />

    <ConfirmModal
      :open="deleteOpen"
      title="Delete switchgear"
      :message="deleteMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @cancel="deleteOpen = false"
      @confirm="confirmDeleteSelected"
    />

  </div>
</template>
