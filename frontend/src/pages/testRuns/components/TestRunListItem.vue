<template>
  <UiMenu ref="menuRef">
    <div
      :class="[
        'cursor-pointer rounded-xl border px-3 py-2 transition-colors',
        active
          ? 'border-blue-500 bg-blue-50 dark:border-blue-400/80 dark:bg-blue-400/10'
          : 'border-transparent hover:border-neutral-300 hover:bg-neutral-50 dark:hover:border-neutral-700 dark:hover:bg-neutral-800'
      ]"
      @contextmenu="openContextMenu($event)"
    >
      <div class="flex items-center justify-between gap-2 text-sm font-semibold text-neutral-900 dark:text-neutral-50">
        <span>Run #{{ run.id }}</span>
        <UiBadge :variant="statusVariant(run.status)">{{ run.status }}</UiBadge>
      </div>
      <div class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
        <span class="uppercase tracking-[0.3em] text-[10px] text-neutral-400 dark:text-neutral-500">{{ snapshotLabel }}</span>
        <span class="mx-1">·</span>
        <span>{{ created }}</span>
      </div>
      <div class="mt-2 text-xs text-neutral-600 dark:text-neutral-300" v-if="sequenceSummary">
        {{ sequenceSummary }}
      </div>
    </div>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-100" @select="repeatRun">
        Repeat
      </UiMenuItem>
      <UiMenuItem
        v-if="run.status !== 'running'"
        class="text-neutral-900 dark:text-neutral-100"
        @select="startRun"
      >
        Start run
      </UiMenuItem>
      <UiMenuItem
        v-else
        class="text-neutral-900 dark:text-neutral-100"
        @select="stopRun"
      >
        Stop run
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { useRouter } from "vue-router"
import UiBadge from "@/components/ui/UiBadge.vue"
import type { TestRunRecord } from "@/types/signal"
import { useTestRunStore } from "@/stores/testRunStore"
import { useToastStore } from "@/stores/toastStore"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  type MenuController,
} from "@affino/menu-vue"

const props = defineProps<{
  run: TestRunRecord
  active?: boolean
  sequenceNameMap: Map<number, string>
}>()

const testRunStore = useTestRunStore()
const toastStore = useToastStore()
const router = useRouter()
const menuRef = ref<{ controller?: MenuController } | null>(null)

const created = computed(() => new Date(props.run.created_at).toLocaleString())
const snapshotLabel = computed(() => (props.run.snapshot ? "SNAPSHOT READY" : "SNAPSHOT PENDING"))

const sequenceSummary = computed(() => {
  if (!props.run.sequence_ids.length) return ""
  const names = props.run.sequence_ids
    .map(id => props.sequenceNameMap.get(id) ?? `Instruction #${id}`)
  if (!names.length) return ""
  return names.slice(0, 2).join(", ") + (names.length > 2 ? ` +${names.length - 2} more` : "")
})

function statusVariant(status: string) {
  switch (status) {
    case "completed":
      return "success"
    case "running":
      return "info"
    case "failed":
      return "danger"
    default:
      return "warning"
  }
}

async function repeatRun() {
  try {
    const clone = await testRunStore.repeatTestRun(props.run.id)
    await router.push({ name: "testRuns.detail", params: { runId: clone.id } })
    toastStore.success(`Created run #${clone.id}`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function startRun() {
  try {
    await testRunStore.startTestRun(props.run.id)
    toastStore.success("Run start requested")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function stopRun() {
  try {
    await testRunStore.stopTestRun(props.run.id)
    toastStore.success("Stop request sent")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function openContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const controller = menuRef.value?.controller
  if (!controller) return
  controller.setAnchor({ x: event.clientX, y: event.clientY, width: 0, height: 0 })
  controller.open("pointer")
}
</script>
