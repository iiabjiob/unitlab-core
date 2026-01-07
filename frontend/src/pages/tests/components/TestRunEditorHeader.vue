<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { TestRunSummary } from "@/types/testRuns"
import { useTestRunStore } from "@/stores/testRunStore"
import UiButton from "@/components/ui/UiButton.vue"
import RenameModal from "@/components/ui/RenameModal.vue"
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
const renameOpen = ref(false)
const renameValue = ref(props.run.name)
const renaming = ref(false)

watch(
  () => props.run.name,
  (value) => {
    if (!renameOpen.value) {
      renameValue.value = value
    }
  },
)

const metaLabel = computed(() => {
  const created = new Date(props.run.created_at)
  return created.toLocaleString()
})

function openRename() {
  renameValue.value = props.run.name
  renameOpen.value = true
}

function cancelRename() {
  renameOpen.value = false
  renameValue.value = props.run.name
}

async function confirmRename() {
  const nextName = renameValue.value.trim()
  if (!nextName || nextName === props.run.name) {
    renameOpen.value = false
    renameValue.value = props.run.name
    return
  }
  renaming.value = true
  try {
    await runStore.updateTestRun(props.run.id, { name: nextName })
    renameOpen.value = false
  } finally {
    renaming.value = false
  }
}
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <div class="flex flex-col gap-1">
      <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white">
        {{ run.name }}
      </div>
      <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500 dark:text-neutral-400">
        <span class="text-xs uppercase tracking-[0.3em] text-neutral-400 dark:text-neutral-500">Test run</span>
        <span>·</span>
        <span>Created {{ metaLabel }}</span>
        <span>·</span>
        <span>Delay {{ run.settings.delay_ms }} ms</span>
      </div>
    </div>

    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>
      <UiMenuContent>
        <UiMenuItem class="text-neutral-800 dark:text-neutral-100" @select="openRename">
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
    title="Rename test run"
    v-model="renameValue"
    :loading="renaming"
    @cancel="cancelRename"
    @confirm="confirmRename"
  />
</template>
