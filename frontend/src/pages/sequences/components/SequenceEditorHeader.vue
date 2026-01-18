<script setup lang="ts">
import { ref, computed, watch } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import type { SequenceDef } from "@/types/sequences"
import UiButton from "@/components/ui/UiButton.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
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
}>()

const store = useSequenceStore()
const renameOpen = ref(false)
const renameValue = ref(props.sequence.name)
const renaming = ref(false)

watch(
  () => props.sequence.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value
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

const usageTooltip = [
  "Execution flow:",
  "This instruction is typically queued inside a Test Run.",
  "Build a run with the instructions you need, add an allocation, and attach a signal list when required.",
  "",
  "• Add the instruction to a Test Run and pick the workspace allocation.",
  "• Assign physical channels or signals in Signals → Test Runs.",
  "• Start or stop execution from Test Runs unless you use direct run controls.",
].join("\n")
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">

    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <div class="flex items-center gap-3">
        <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white">
          {{ sequence.name }}
        </div>
        <!-- <UiBadge variant="info" :title="usageTooltip">Used in Test Runs</UiBadge> -->
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
        <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="promptRename">
          Rename
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
</template>
