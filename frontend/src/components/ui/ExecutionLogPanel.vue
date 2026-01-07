<script setup lang="ts">
import { computed, ref } from "vue"
import { useAutoScroll } from "@/composables/useAutoScroll"

const defaultColors: Record<string, { dot: string; text?: string }> = {
  error: { dot: "bg-red-500", text: "text-red-400" },
  step: { dot: "bg-blue-400", text: "text-blue-300" },
  command: { dot: "bg-blue-400", text: "text-blue-300" },
  info: { dot: "bg-neutral-400", text: "text-neutral-700 dark:text-neutral-300" },
}

const props = withDefaults(defineProps<{
  title?: string
  logs: Array<{ ts: string; type: string; message: string }>
  typeColors?: Record<string, { dot?: string; text?: string }>
  emptyMessage?: string
}>(), {
  title: "Execution Log",
  logs: () => [],
  emptyMessage: "No logs yet…",
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
</script>

<template>
  <div class="h-full flex flex-col select-none">
    <div class="p-4 text-xs uppercase tracking-wider text-neutral-500 dark:text-neutral-400 border-b border-neutral-200 dark:border-neutral-800">
      {{ props.title }}
    </div>
    <div
      ref="logContainer"
      class="flex-1 overflow-y-auto px-4 py-2 space-y-0.5 font-mono text-[11px] leading-tight text-neutral-700 dark:text-neutral-300"
    >
      <div
        v-for="(log, index) in entries"
        :key="index"
        class="flex items-center gap-2 py-px px-1 rounded-sm hover:bg-neutral-100 dark:hover:bg-neutral-800/50 transition-colors"
      >
        <div class="text-[10px] opacity-50 w-20 shrink-0 text-left">
          {{ log.ts }}
        </div>
        <div class="w-2 h-2 rounded-full" :class="dotClass(log.type)" />
        <div class="whitespace-pre-wrap break-words flex-1" :class="textClass(log.type)">
          {{ log.message }}
        </div>
      </div>
      <div v-if="entries.length === 0" class="opacity-40 italic py-2">
        {{ props.emptyMessage }}
      </div>
    </div>
  </div>
</template>
