<script setup lang="ts">
import { ref, computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import type { SequenceDef } from "@/types/sequences"
import UiButton from "@/components/ui/UiButton.vue"

const props = defineProps<{
  sequence: SequenceDef
}>()

const store = useSequenceStore()

const editing = ref(false)
const tempName = ref(props.sequence.name)

function startEdit() {
  editing.value = true
  tempName.value = props.sequence.name
}

async function saveEdit() {
  editing.value = false
  if (tempName.value.trim() && tempName.value !== props.sequence.name) {
    await store.updateSequence(props.sequence.id, { name: tempName.value })
  }
}

function cancelEdit() {
  editing.value = false
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

      <!-- Sequence name -->
      <div class="flex items-center gap-2">
        <div
          v-if="!editing"
          class="text-lg font-medium tracking-tight hover:text-blue-400 cursor-pointer"
          @dblclick="startEdit"
        >
          {{ sequence.name }}
        </div>

        <input
          v-else
          v-model="tempName"
          @keydown.enter="saveEdit"
          @keydown.esc="cancelEdit"
          @blur="saveEdit"
          class="px-2 py-1 text-sm rounded bg-neutral-800 border border-neutral-600 
                 text-neutral-200 focus:ring-1 focus:ring-blue-500"
          autofocus
        />
      </div>

      <!-- Metadata -->
      <div class="text-xs text-neutral-500 leading-normal">
        <template v-if="sequence.description">
          <div>{{ sequence.description }}</div>
        </template>
        <div class="opacity-70">Created: {{ createdAt }}</div>
      </div>
    </div>

    <!-- RIGHT ACTIONS -->
    <div class="flex items-center gap-2">

      <UiButton variant="secondary" size="xs">
        Duplicate
      </UiButton>

      <UiButton variant="danger" size="xs">
        Delete
      </UiButton>

    </div>
  </div>
</template>
