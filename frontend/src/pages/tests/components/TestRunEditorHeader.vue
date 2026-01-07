<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { TestRunSummary } from "@/types/testRuns"
import { useTestRunStore } from "@/stores/testRunStore"
import UiButton from "@/components/ui/UiButton.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"

const props = defineProps<{ run: TestRunSummary }>()
const emit = defineEmits<{ (e: "delete"): void; (e: "duplicate"): void }>()

const runStore = useTestRunStore()
const editing = ref(false)
const nameInput = ref(props.run.name)
const saving = ref(false)

watch(
  () => props.run.name,
  (value) => {
    if (!editing.value) {
      nameInput.value = value
    }
  },
)

const metaLabel = computed(() => {
  const created = new Date(props.run.created_at)
  return created.toLocaleString()
})

function startEdit() {
  editing.value = true
  nameInput.value = props.run.name
}

function cancelEdit() {
  editing.value = false
  nameInput.value = props.run.name
}

async function saveName() {
  if (!editing.value) return
  const nextName = nameInput.value.trim()
  editing.value = false
  if (!nextName || nextName === props.run.name) {
    nameInput.value = props.run.name
    return
  }
  saving.value = true
  try {
    await runStore.updateTestRun(props.run.id, { name: nextName })
  } finally {
    saving.value = false
  }
}
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <div class="flex flex-col gap-1">
      <div class="flex items-center gap-2">
        <button
          v-if="!editing"
          type="button"
          class="text-lg font-medium tracking-tight hover:text-blue-400"
          :disabled="saving"
          @dblclick="startEdit"
        >
          {{ run.name }}
        </button>
        <input
          v-else
          v-model="nameInput"
          class="px-2 py-1 text-sm rounded bg-neutral-50 border border-neutral-300 focus:outline-none focus:ring-1 focus:ring-blue-500 dark:bg-neutral-900 dark:border-neutral-600"
          :disabled="saving"
          @blur="saveName"
          @keydown.enter.prevent="saveName"
          @keydown.esc.prevent="cancelEdit"
        />
        <span class="text-xs uppercase tracking-[0.3em] text-neutral-400 dark:text-neutral-500">Test run</span>
      </div>
      <div class="text-xs text-neutral-500 dark:text-neutral-400">
        Created {{ metaLabel }} · Delay {{ run.settings.delay_ms }} ms
      </div>
    </div>

    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>
      <UiMenuContent>
        <UiMenuItem class="text-neutral-800 dark:text-neutral-100" @select="startEdit">
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
</template>
