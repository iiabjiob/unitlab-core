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
      <SidebarListItem :active="active" class="sequence-list-item" @select="handleSelect">
        <span class="sequence-list-item__name">
          {{ sequence.name }}
        </span>
      </SidebarListItem>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="sequence-list-item__menu-item" @select="openInNewTab">
        Open in new tab
      </UiMenuItem>
      <UiMenuItem class="sequence-list-item__menu-item" @select="exportSequence">
        Export
      </UiMenuItem>
      <UiMenuItem class="sequence-list-item__menu-item" @select="openRename">
        Rename
      </UiMenuItem>
      <UiMenuItem class="sequence-list-item__menu-item" @select="openDescriptionEditor">
        Edit description
      </UiMenuItem>
      <UiMenuItem class="sequence-list-item__menu-item" @select="duplicateSequence">
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
    <label class="sequence-list-item__description-label" for="sequence-description">
      Description
    </label>
    <textarea
      id="sequence-description"
      name="sequence-description"
      v-model="descriptionValue"
      data-dialog-initial
      rows="6"
      class="sequence-list-item__description-input"
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

<style scoped>
.sequence-list-item {
  position: relative;
  border-radius: var(--radius-md);
  background: transparent;
  box-shadow: inset 0 0 0 1px transparent;
  color: var(--color-neutral-700);
}

.sequence-list-item.sidebar-list-item:not(.is-active):hover {
  background: color-mix(in srgb, var(--color-white) 72%, var(--color-neutral-100));
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
  color: var(--color-neutral-900);
}

.sequence-list-item.sidebar-list-item.is-active {
  background:
    linear-gradient(
      180deg,
      color-mix(in srgb, var(--color-blue-100) 52%, var(--color-white)),
      color-mix(in srgb, var(--color-white) 84%, var(--color-blue-100))
    );
  box-shadow:
    inset 0 0 0 1px color-mix(in srgb, var(--color-blue-500) 34%, var(--color-neutral-200)),
    0 8px 18px color-mix(in srgb, var(--color-blue-500) 8%, transparent);
  color: var(--color-neutral-900);
}

.sequence-list-item__name {
  overflow: hidden;
  font-size: var(--text-sm);
  font-weight: 500;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.sequence-list-item.sidebar-list-item:not(.is-active) :deep(.sidebar-list-item__indicator) {
  background: color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
}

.sequence-list-item__menu-item {
  color: var(--color-neutral-900);
}

.sequence-list-item__description-label {
  display: block;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.sequence-list-item__description-input {
  width: 100%;
  margin-top: 0.5rem;
  padding: 0.5rem 0.75rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-sm);
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  outline: none;
}

.sequence-list-item__description-input::placeholder {
  color: var(--color-neutral-500);
}

.sequence-list-item__description-input:focus {
  box-shadow: 0 0 0 1px var(--color-blue-500);
}

.sequence-list-item__description-input:disabled {
  opacity: 0.6;
}

:global(.dark .sequence-list-item__menu-item) {
  color: var(--color-neutral-200);
}

:global(.dark) .sequence-list-item.sidebar-list-item {
  background: transparent;
  box-shadow: inset 0 0 0 1px transparent;
  color: var(--color-neutral-200);
}

:global(.dark) .sequence-list-item.sidebar-list-item:not(.is-active):hover {
  background:
    linear-gradient(
      180deg,
      color-mix(in srgb, var(--color-neutral-800) 54%, var(--color-neutral-900)),
      color-mix(in srgb, var(--color-neutral-900) 90%, var(--color-neutral-800))
    );
  box-shadow: inset 0 0 0 1px color-mix(in srgb, var(--color-blue-400) 20%, var(--color-neutral-700));
  color: var(--color-neutral-50);
}

:global(.dark) .sequence-list-item.sidebar-list-item.is-active {
  background:
    linear-gradient(
      180deg,
      color-mix(in srgb, var(--color-blue-500) 20%, var(--color-neutral-900)),
      color-mix(in srgb, var(--color-blue-900) 18%, var(--color-neutral-950))
    );
  box-shadow:
    inset 0 0 0 1px color-mix(in srgb, var(--color-blue-400) 40%, var(--color-neutral-700)),
    0 10px 22px color-mix(in srgb, var(--color-blue-900) 20%, transparent);
  color: var(--color-white);
}

:global(.dark) .sequence-list-item.sidebar-list-item:not(.is-active) :deep(.sidebar-list-item__indicator) {
  background: color-mix(in srgb, var(--color-neutral-700) 78%, transparent);
}

:global(.dark .sequence-list-item__description-label) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-list-item__description-input) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}
</style>
