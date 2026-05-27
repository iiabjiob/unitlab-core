<script setup lang="ts">
import { ref, computed, watch } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import type { SequenceDef } from "@/types/sequences"
import UiButton from "@/components/ui/UiButton.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
import UiModal from "@/components/ui/UiModal.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@/components/ui/menu"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

const props = defineProps<{
  sequence: SequenceDef
}>()

const emit = defineEmits<{
  (e: "duplicate"): void
  (e: "delete"): void
  (e: "export"): void
}>()

const store = useSequenceStore()
const toastStore = useToastStore()
const renameOpen = ref(false)
const renameValue = ref(props.sequence.name)
const renaming = ref(false)
const descriptionOpen = ref(false)
const descriptionValue = ref(props.sequence.description ?? "")
const savingDescription = ref(false)

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

function promptRename() {
  renameValue.value = props.sequence.name
  renameOpen.value = true
}

function closeRename() {
  renameOpen.value = false
  renameValue.value = props.sequence.name
}

function openDescriptionEditor() {
  descriptionValue.value = props.sequence.description ?? ""
  descriptionOpen.value = true
}

function closeDescriptionEditor() {
  descriptionOpen.value = false
  descriptionValue.value = props.sequence.description ?? ""
}

async function confirmRename() {
  const next = renameValue.value.trim()
  if (!next || next === props.sequence.name) {
    renameOpen.value = false
    renameValue.value = props.sequence.name
    return
  }

  renaming.value = true
  try {
    await store.updateSequence(props.sequence.id, { name: next })
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

const createdAt = computed(() => {
  const d = new Date(props.sequence.created_at)
  return d.toLocaleString("en-GB", {
    year: "numeric",
    month: "short",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  })
})

</script>

<template>
  <div class="sequence-editor-header">
    <div class="sequence-editor-header__main">
      <div class="sequence-editor-header__title-row">
        <div class="sequence-editor-header__title">
          {{ sequence.name }}
        </div>
        <UiBadge
          v-if="sequence.read_only"
          variant="warning"
          title="This instruction is managed by the system and cannot be edited"
        >
          Read-only
        </UiBadge>
      </div>

      <div class="sequence-editor-header__meta">
        <span class="sequence-editor-header__eyebrow">
          Instruction
        </span>
        <span>·</span>
        <span>Created {{ createdAt }}</span>
      </div>

      <div v-if="sequence.description" class="sequence-editor-header__description">
        {{ sequence.description }}
      </div>
    </div>

    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon" aria-label="Instruction actions">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>

      <UiMenuContent>
        <UiMenuItem class="sequence-editor-header__menu-item" @select="emit('export')">
          Export
        </UiMenuItem>
        <UiMenuItem class="sequence-editor-header__menu-item" @select="promptRename">
          Rename
        </UiMenuItem>
        <UiMenuItem class="sequence-editor-header__menu-item" @select="openDescriptionEditor">
          Edit description
        </UiMenuItem>
        <UiMenuItem class="sequence-editor-header__menu-item" @select="emit('duplicate')">
          Duplicate
        </UiMenuItem>

        <UiMenuItem danger @select="emit('delete')">
          Delete
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
  </div>

  <RenameModal
    :open="renameOpen"
    title="Rename instruction"
    v-model="renameValue"
    :loading="renaming"
    @cancel="closeRename"
    @confirm="confirmRename"
  />

  <UiModal :open="descriptionOpen" title="Edit instruction description" @close="closeDescriptionEditor">
    <label class="sequence-editor-header__description-label" for="sequence-header-description">
      Description
    </label>
    <textarea
      id="sequence-header-description"
      name="sequence-header-description"
      v-model="descriptionValue"
      data-dialog-initial
      rows="6"
      class="sequence-editor-header__description-input"
      :disabled="savingDescription"
      placeholder="Add instruction description"
    />

    <template #footer>
      <button
        type="button"
        class="btn btn-secondary btn-base"
        :disabled="savingDescription"
        @click="closeDescriptionEditor"
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
</template>

<style scoped>
.sequence-editor-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  padding: 0.75rem 1rem;
  border-bottom: 1px solid var(--color-neutral-300);
}

.sequence-editor-header__main {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.sequence-editor-header__title-row {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.sequence-editor-header__title {
  color: var(--color-neutral-900);
  font-size: var(--text-lg);
  font-weight: 500;
  letter-spacing: 0;
}

.sequence-editor-header__meta {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.75rem;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.sequence-editor-header__eyebrow {
  color: var(--color-neutral-400);
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.sequence-editor-header__description {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.sequence-editor-header__menu-item {
  color: var(--color-neutral-900);
}

.sequence-editor-header__description-label {
  display: block;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.sequence-editor-header__description-input {
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

.sequence-editor-header__description-input::placeholder {
  color: var(--color-neutral-500);
}

.sequence-editor-header__description-input:focus {
  box-shadow: 0 0 0 1px var(--color-blue-500);
}

.sequence-editor-header__description-input:disabled {
  opacity: 0.6;
}

:global(.dark .sequence-editor-header) {
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .sequence-editor-header__title) {
  color: var(--color-white);
}

:global(.dark .sequence-editor-header__meta),
:global(.dark .sequence-editor-header__description),
:global(.dark .sequence-editor-header__description-label) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-editor-header__eyebrow) {
  color: var(--color-neutral-500);
}

:global(.dark .sequence-editor-header__menu-item) {
  color: var(--color-neutral-200);
}

:global(.dark .sequence-editor-header__description-input) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}
</style>
