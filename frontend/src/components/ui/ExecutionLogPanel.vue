<script setup lang="ts">
import { computed, ref } from "vue"
import { useAutoScroll } from "@/composables/useAutoScroll"

type LogEntry = {
  ts: string
  type: string
  message: string
  [key: string]: any
}

const defaultColors: Record<string, { dot: string; text?: string }> = {
  error: { dot: "bg-red-500", text: "text-red-400" },
  step: { dot: "bg-blue-400", text: "text-blue-300" },
  command: { dot: "bg-blue-400", text: "text-blue-300" },
  info: { dot: "bg-neutral-400", text: "text-neutral-700 dark:text-neutral-300" },
}

const emit = defineEmits<{
  (e: "select", payload: { index: number; log: LogEntry }): void
}>()

const props = withDefaults(defineProps<{
  title?: string
  logs: LogEntry[]
  typeColors?: Record<string, { dot?: string; text?: string }>
  emptyMessage?: string
  selectable?: boolean
  selectedIndex?: number | null
}>(), {
  title: "Execution Log",
  logs: () => [],
  emptyMessage: "No logs yet…",
  selectable: false,
  selectedIndex: null,
})

const entries = computed(() => props.logs ?? [])
const logContainer = ref<HTMLElement | null>(null)
useAutoScroll(entries, logContainer)

function dotClass(type: string) {
  return props.typeColors?.[type]?.dot ?? defaultColors[type]?.dot ?? "bg-neutral-400"
}

function textClass(type: string) {
  return props.typeColors?.[type]?.text ?? defaultColors[type]?.text ?? ""
}

function detailText(log: LogEntry): string {
  return log.details ?? log.step_label ?? ""
}

function handleSelect(index: number) {
  if (!props.selectable) return
  const log = entries.value[index]
  if (!log) return
  emit("select", { index, log })
}

function onKeydown(event: KeyboardEvent, index: number) {
  if (!props.selectable) return
  if (event.key === "Enter" || event.key === " ") {
    event.preventDefault()
    handleSelect(index)
  }
}
</script>

<template>
  <div class="h-full min-h-0 flex flex-col select-none">
    <div class="p-4 text-xs uppercase tracking-wider text-neutral-500 dark:text-neutral-400 border-b border-neutral-200 dark:border-neutral-800">
      {{ props.title }}
    </div>
    <div
      ref="logContainer"
      class="flex-1 min-h-0 overflow-y-auto px-4 py-2 space-y-0.5 font-mono text-[11px] leading-tight text-neutral-700 dark:text-neutral-300"
    >
      <div
        v-for="(log, index) in entries"
        :key="index"
        class="flex items-center gap-2 py-1 px-1 rounded-sm transition-colors"
        :class="[
          props.selectable ? 'cursor-pointer hover:bg-neutral-100 dark:hover:bg-neutral-800/50 focus:outline-none focus:ring-1 focus:ring-neutral-300 dark:focus:ring-neutral-600' : 'hover:bg-neutral-100 dark:hover:bg-neutral-800/50',
          props.selectedIndex === index ? 'bg-neutral-100 dark:bg-neutral-800/60' : '',
        ]"
        :tabindex="props.selectable ? 0 : undefined"
        @click="handleSelect(index)"
        @keydown="onKeydown($event, index)"
      >
        <div class="text-[10px] opacity-50 w-20 shrink-0 text-left">
          {{ log.ts }}
        </div>
        <div class="w-2 h-2 rounded-full" :class="dotClass(log.type)" />
        <div class="whitespace-pre-wrap break-words flex-1" :class="textClass(log.type)">
          <span>{{ log.message }}</span>
          <span
            v-if="detailText(log)"
            class="ml-1 text-[10px] text-neutral-500 dark:text-neutral-400"
          >
            · {{ detailText(log) }}
          </span>
        </div>
      </div>
      <div v-if="entries.length === 0" class="opacity-40 italic py-2">
        {{ props.emptyMessage }}
      </div>
    </div>
  </div>
</template>
