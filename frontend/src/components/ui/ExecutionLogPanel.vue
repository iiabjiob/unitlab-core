<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from "vue"
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
const copyButtonText = ref("Copy log")
let copyFeedbackTimeout: number | null = null

function setCopyButtonFeedback(text: string, durationMs = 1500) {
  copyButtonText.value = text
  if (copyFeedbackTimeout !== null) {
    window.clearTimeout(copyFeedbackTimeout)
  }
  copyFeedbackTimeout = window.setTimeout(() => {
    copyButtonText.value = "Copy log"
    copyFeedbackTimeout = null
  }, durationMs)
}

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

function toLine(log: LogEntry): string {
  const detail = detailText(log)
  const detailsSuffix = detail ? ` · ${detail}` : ""
  return `[${log.ts}] ${String(log.type).toUpperCase()}: ${log.message}${detailsSuffix}`
}

const logText = computed(() => entries.value.map(toLine).join("\n"))

async function copyLogs() {
  if (!entries.value.length) {
    setCopyButtonFeedback("No logs")
    return
  }

  try {
    if (typeof navigator !== "undefined" && navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(logText.value)
    } else if (typeof document !== "undefined") {
      const textarea = document.createElement("textarea")
      textarea.value = logText.value
      textarea.style.position = "fixed"
      textarea.style.opacity = "0"
      document.body.appendChild(textarea)
      textarea.focus()
      textarea.select()
      document.execCommand("copy")
      document.body.removeChild(textarea)
    } else {
      throw new Error("Clipboard API unavailable")
    }
    setCopyButtonFeedback("Copied")
  } catch {
    setCopyButtonFeedback("Failed")
  }
}

onBeforeUnmount(() => {
  if (copyFeedbackTimeout !== null) {
    window.clearTimeout(copyFeedbackTimeout)
    copyFeedbackTimeout = null
  }
})
</script>

<template>
  <div class="flex flex-col select-none lg:h-full lg:min-h-0">
    <div class="p-4 text-xs uppercase tracking-wider text-neutral-500 dark:text-neutral-400 border-b border-neutral-200 dark:border-neutral-800 flex items-center justify-between gap-3">
      <span>{{ props.title }}</span>
      <button
        type="button"
        class="relative text-[10px] px-2 py-1 rounded border border-neutral-300 text-neutral-600 hover:bg-neutral-100 dark:border-neutral-700 dark:text-neutral-300 dark:hover:bg-neutral-800/60 transition-colors"
        @click="copyLogs"
      >
        <span class="invisible">Copy log</span>
        <span class="absolute inset-0 flex items-center justify-center">
          {{ copyButtonText }}
        </span>
      </button>
    </div>
    <div
      ref="logContainer"
      class="px-4 py-2 space-y-0.5 font-mono text-[11px] leading-tight text-neutral-700 dark:text-neutral-300 lg:flex-1 lg:min-h-0 lg:overflow-y-auto"
    >
      <div
        v-for="(log, index) in entries"
        :key="index"
        class="flex items-center gap-2 py-1 px-1 rounded-sm transition-colors"
        :class="[
          props.selectable ? 'cursor-default hover:bg-neutral-100 dark:hover:bg-neutral-800/50 focus:outline-none focus:ring-1 focus:ring-neutral-300 dark:focus:ring-neutral-600' : 'hover:bg-neutral-100 dark:hover:bg-neutral-800/50',
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
