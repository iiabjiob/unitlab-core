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
  UiMenuItem
} from "@affino/menu-vue"
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
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">

    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <div class="flex items-center gap-3">
        <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white">
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

      <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500 dark:text-neutral-400">
        <span class="text-xs uppercase tracking-[0.3em] text-neutral-400 dark:text-neutral-500">
          Instruction
        </span>
        <span>·</span>
        <span>Created {{ createdAt }}</span>
      </div>

      <div v-if="sequence.description" class="text-xs text-neutral-500 dark:text-neutral-400">
        {{ sequence.description }}
      </div>
    </div>

    <!-- RIGHT ACTIONS -->
    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon">
          <EllipsisHorizontalIcon size="24"/>
        </UiButton>
      </UiMenuTrigger>

      <UiMenuContent>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('export')">
          Export
        </UiMenuItem>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="promptRename">
          Rename
        </UiMenuItem>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="openDescriptionEditor">
          Edit description
        </UiMenuItem>
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('duplicate')">
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
    <label class="block text-xs uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400" for="sequence-header-description">
      Description
    </label>
    <textarea
      id="sequence-header-description"
      name="sequence-header-description"
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
