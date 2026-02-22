<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiModal from "@/components/ui/UiModal.vue"
import type { SequenceDef } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"

const props = defineProps<{
  sequence: SequenceDef
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()

const store = useSequenceStore()
const toastStore = useToastStore()
const router = useRouter()
const route = useRoute()

const renameOpen = ref(false)
const renameValue = ref(props.sequence.name)
const renaming = ref(false)
const descriptionOpen = ref(false)
const descriptionValue = ref(props.sequence.description ?? "")
const savingDescription = ref(false)
const deleteOpen = ref(false)

watch(
  () => props.sequence.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value
    }
  },
)

watch(
  () => props.sequence.description,
  (value) => {
    if (!descriptionOpen.value) {
      descriptionValue.value = value ?? ""
    }
  },
)

const deleteMessage = computed(() => `Instruction "${props.sequence.name}" will be deleted with all steps.`)

function handleSelect() {
  emit("select", props.sequence.id)
}

function openRename() {
  renameValue.value = props.sequence.name
  renameOpen.value = true
}

function cancelRename() {
  renameOpen.value = false
  renameValue.value = props.sequence.name
}

function openDescriptionEditor() {
  descriptionValue.value = props.sequence.description ?? ""
  descriptionOpen.value = true
}

function cancelDescriptionEditor() {
  descriptionOpen.value = false
  descriptionValue.value = props.sequence.description ?? ""
}

async function confirmRename() {
  const trimmed = renameValue.value.trim()
  if (!trimmed || trimmed === props.sequence.name) {
    renameOpen.value = false
    return
  }

  renaming.value = true
  try {
    await store.updateSequence(props.sequence.id, { name: trimmed })
    renameOpen.value = false
  } finally {
    renaming.value = false
  }
}

async function confirmDescription() {
  const nextDescription = descriptionValue.value.trim()
  const currentDescription = (props.sequence.description ?? "").trim()

  if (nextDescription === currentDescription) {
    descriptionOpen.value = false
    return
  }

  descriptionOpen.value = false
  savingDescription.value = true
  try {
    await store.updateSequence(props.sequence.id, { description: nextDescription })
  } catch (error) {
    descriptionOpen.value = true
    descriptionValue.value = nextDescription
    toastStore.error(error instanceof Error ? error.message : "Failed to save description")
  } finally {
    savingDescription.value = false
  }
}

async function duplicateSequence() {
  const duplicated = await store.duplicateSequence(props.sequence.id)
  await router.push({ name: "instructions.detail", params: { id: duplicated.id } })
}

async function exportSequence() {
  try {
    const payload = await store.exportSequenceFile(props.sequence.id)
    const json = JSON.stringify(payload, null, 2)
    const blob = new Blob([json], { type: "application/json;charset=utf-8" })
    const url = URL.createObjectURL(blob)
    const link = document.createElement("a")
    link.href = url
    const safeName = props.sequence.name.replace(/[^a-zA-Z0-9_-]/g, "_")
    link.download = `${safeName || "sequence"}_${props.sequence.id}.json`
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    URL.revokeObjectURL(url)
    toastStore.success("Instruction exported")
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to export instruction")
  }
}

async function confirmDelete() {
  const before = [...store.sequences]
  const currentIndex = before.findIndex(item => item.id === props.sequence.id)

  await store.deleteSequence(props.sequence.id)
  deleteOpen.value = false
  if (Number(route.params.id) === props.sequence.id) {
    const after = store.sequences
    if (after.length === 0) {
      await router.push({ name: "instructions.list" })
      return
    }

    const fallbackIndex = currentIndex < 0
      ? 0
      : Math.min(currentIndex, after.length - 1)
    const fallback = after[fallbackIndex]
    await router.push({ name: "instructions.detail", params: { id: fallback.id } })
  }
}

function openInNewTab() {
  const resolved = router.resolve({ name: "instructions.detail", params: { id: props.sequence.id } })
  if (typeof window !== "undefined") {
    window.open(resolved.href, "_blank", "noopener,noreferrer")
  }
}
</script>

<template>
  <UiMenu>
    <UiMenuTrigger as-child trigger="contextmenu">
    <SidebarListItem :active="active" class="relative" @select="handleSelect">
      <span class="truncate text-sm">
        {{ sequence.name }}
      </span>
    </SidebarListItem>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openInNewTab">
        Open in new tab
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="exportSequence">
        Export
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openRename">
        Rename
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openDescriptionEditor">
        Edit description
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="duplicateSequence">
        Duplicate
      </UiMenuItem>
      <UiMenuItem danger @select="deleteOpen = true">
        Delete
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>

  <RenameModal
    :open="renameOpen"
    title="Rename instruction"
    v-model="renameValue"
    :loading="renaming"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />

  <UiModal :open="descriptionOpen" title="Edit instruction description" @close="cancelDescriptionEditor">
    <label class="block text-xs uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400" for="sequence-description">
      Description
    </label>
    <textarea
      id="sequence-description"
      name="sequence-description"
      v-model="descriptionValue"
      data-dialog-initial
      rows="6"
      class="mt-2 w-full rounded border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none focus:ring-1 focus:ring-blue-500 disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      :disabled="savingDescription"
      placeholder="Add instruction description"
    />

    <template #footer>
      <button
        type="button"
        class="btn btn-secondary btn-base"
        :disabled="savingDescription"
        @click="cancelDescriptionEditor"
      >
        Cancel
      </button>
      <button
        type="button"
        class="btn btn-primary btn-base"
        :disabled="savingDescription"
        @click="confirmDescription"
      >
        {{ savingDescription ? "Saving…" : "Save" }}
      </button>
    </template>
  </UiModal>

  <ConfirmModal
    :open="deleteOpen"
    title="Delete instruction"
    :message="deleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="deleteOpen = false"
    @confirm="confirmDelete"
  />
</template>