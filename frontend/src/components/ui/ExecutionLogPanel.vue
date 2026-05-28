<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { useAutoScroll } from "@/composables/useAutoScroll"
import { useVirtualList } from "@/composables/useVirtualList"

type LogEntry = {
  ts: string
  type: string
  message: string
  [key: string]: any
}

const defaultColors: Record<string, { dot: string; text?: string }> = {
  error: { dot: "execution-log__dot--error", text: "execution-log__message--error" },
  step: { dot: "execution-log__dot--step", text: "execution-log__message--step" },
  command: { dot: "execution-log__dot--step", text: "execution-log__message--step" },
  info: { dot: "execution-log__dot--info", text: "execution-log__message--info" },
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
const virtualLog = useVirtualList(entries, logContainer, {
  estimateSize: 28,
  overscan: 10,
})
const autoScroll = useAutoScroll(entries, logContainer)
const copyButtonText = ref("Copy log")
let copyFeedbackTimeout: number | null = null

watch(virtualLog.totalSize, () => {
  autoScroll.scheduleScroll()
})

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
  return props.typeColors?.[type]?.dot ?? defaultColors[type]?.dot ?? "execution-log__dot--info"
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

function setVirtualRowElement(index: number, element: unknown) {
  virtualLog.setItemElement(index, element instanceof HTMLElement ? element : null)
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
  <div class="execution-log">
    <div class="execution-log__header">
      <span>{{ props.title }}</span>
      <button
        type="button"
        class="btn btn-xs btn-secondary execution-log__copy"
        @click="copyLogs"
      >
        <span class="execution-log__copy-spacer">Copy log</span>
        <span class="execution-log__copy-label">
          {{ copyButtonText }}
        </span>
      </button>
    </div>
    <div
      ref="logContainer"
      class="execution-log__list"
    >
      <div
        v-if="entries.length > 0"
        class="execution-log__virtual-window"
        :style="{
          paddingTop: `${virtualLog.topSpacer.value}px`,
          paddingBottom: `${virtualLog.bottomSpacer.value}px`,
        }"
      >
        <div
          v-for="row in virtualLog.virtualItems.value"
          :key="row.index"
          :ref="element => setVirtualRowElement(row.index, element)"
          class="execution-log__row"
          :class="{
            'is-selectable': props.selectable,
            'is-selected': props.selectedIndex === row.index,
          }"
          :tabindex="props.selectable ? 0 : undefined"
          @click="handleSelect(row.index)"
          @keydown="onKeydown($event, row.index)"
        >
          <div class="execution-log__time">
            {{ row.item.ts }}
          </div>
          <div class="execution-log__dot" :class="dotClass(row.item.type)" />
          <div class="execution-log__message" :class="textClass(row.item.type)">
            <span>{{ row.item.message }}</span>
            <span
              v-if="detailText(row.item)"
              class="execution-log__detail"
            >
              · {{ detailText(row.item) }}
            </span>
          </div>
        </div>
      </div>
      <div v-if="entries.length === 0" class="execution-log__empty">
        {{ props.emptyMessage }}
      </div>
    </div>
  </div>
</template>

<style scoped>
.execution-log {
  display: flex;
  flex: 1 1 auto;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: var(--color-neutral-50);
  user-select: none;
}

.execution-log__header {
  align-items: center;
  border-bottom: 1px solid var(--color-neutral-200);
  color: var(--color-neutral-500);
  display: flex;
  font-size: var(--text-xs);
  gap: 0.75rem;
  justify-content: space-between;
  letter-spacing: 0.05em;
  line-height: 1rem;
  padding: 1rem;
  text-transform: uppercase;
}

.execution-log__copy {
  position: relative;
}

.execution-log__copy-spacer {
  visibility: hidden;
}

.execution-log__copy-label {
  align-items: center;
  display: flex;
  inset: 0;
  justify-content: center;
  position: absolute;
}

.execution-log__list {
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  color: var(--color-neutral-700);
  font-family: var(--font-mono);
  font-size: 0.6875rem;
  line-height: 1.25;
  padding: 0.5rem 1rem;
}

.execution-log__virtual-window {
  box-sizing: border-box;
  min-height: 100%;
}

.execution-log__row {
  align-items: flex-start;
  border-radius: var(--radius-sm);
  display: flex;
  gap: 0.5rem;
  padding: 0.25rem;
  transition: background-color 150ms ease, box-shadow 150ms ease;
}

.execution-log__row:hover,
.execution-log__row.is-selected {
  background: var(--color-neutral-100);
}

.execution-log__row.is-selectable {
  cursor: pointer;
}

.execution-log__row.is-selectable:focus {
  outline: none;
}

.execution-log__row.is-selectable:focus-visible {
  box-shadow: 0 0 0 1px var(--color-neutral-300);
}

.execution-log__time {
  flex: 0 0 auto;
  font-size: 0.625rem;
  opacity: 0.5;
  text-align: left;
  width: 5rem;
}

.execution-log__dot {
  align-self: flex-start;
  border-radius: 999px;
  flex: 0 0 0.5rem;
  height: 0.5rem;
  margin-top: 0.1875rem;
  min-height: 0.5rem;
  min-width: 0.5rem;
  width: 0.5rem;
}

.execution-log__dot--error {
  background: var(--color-red-500);
}

.execution-log__dot--step {
  background: var(--color-blue-400);
}

.execution-log__dot--info {
  background: var(--color-neutral-400);
}

.execution-log__message {
  flex: 1 1 auto;
  overflow-wrap: anywhere;
  white-space: pre-wrap;
}

.execution-log__message--error {
  color: var(--color-red-400);
}

.execution-log__message--step {
  color: var(--color-blue-300);
}

.execution-log__message--info {
  color: var(--color-neutral-700);
}

.execution-log__detail {
  color: var(--color-neutral-500);
  font-size: 0.625rem;
  margin-left: 0.25rem;
}

.execution-log__empty {
  font-style: italic;
  opacity: 0.4;
  padding: 0.5rem 0;
}

:global(.dark .execution-log) {
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-900) 60%, transparent);
}

:global(.dark .execution-log__header) {
  border-color: var(--color-neutral-800);
  color: var(--color-neutral-400);
}

:global(.dark .execution-log__list),
:global(.dark .execution-log__message--info) {
  color: var(--color-neutral-300);
}

:global(.dark .execution-log__row:hover) {
  background: color-mix(in srgb, var(--color-neutral-800) 50%, transparent);
}

:global(.dark .execution-log__row.is-selected) {
  background: color-mix(in srgb, var(--color-neutral-800) 60%, transparent);
}

:global(.dark .execution-log__row.is-selectable:focus-visible) {
  box-shadow: 0 0 0 1px var(--color-neutral-600);
}

:global(.dark .execution-log__detail) {
  color: var(--color-neutral-400);
}
</style>
