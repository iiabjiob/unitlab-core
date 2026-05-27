<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from "vue"
import { useChannelLogStore } from "@/stores/channelLogStore"
import type { Device } from "@/types/device"
import { useAutoScroll } from "@/composables/useAutoScroll"

const props = defineProps<{ device: Device }>()

const logStore = useChannelLogStore()
const logs = computed(() => logStore.logs[props.device.id] ?? [])

const logContainer = ref<HTMLElement | null>(null)
useAutoScroll(logs, logContainer)
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

function toLine(log: (typeof logs.value)[number]): string {
  const action = log.actionId ? ` #${log.actionId}` : ""
  const reason = log.reason ? ` (reason: ${log.reason})` : ""
  return `[${log.ts}] ${String(log.type).toUpperCase()}${action}: ${log.message}${reason}`
}

const logText = computed(() => logs.value.map(toLine).join("\n"))

async function copyLogs() {
  if (!logs.value.length) {
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
  <div class="device-execution-log">

    <div class="device-execution-log__header">
      <span>Device Log</span>
      <button
        type="button"
        class="btn btn-xs btn-secondary device-execution-log__copy-button"
        @click="copyLogs"
      >
        <span class="device-execution-log__copy-label">Copy log</span>
        <span class="device-execution-log__copy-feedback">
          {{ copyButtonText }}
        </span>
      </button>
    </div>

    <div
      ref="logContainer"
      class="device-execution-log__body"
    >

      <template v-for="(log, i) in logs" :key="i">
        <div
          v-if="log.actionId && (i === 0 || log.actionId !== logs[i - 1]?.actionId)"
          class="device-execution-log__action-heading"
        >
          Action {{ log.actionId }}
        </div>

        <div
          class="device-execution-log__row"
        >
          <div class="device-execution-log__time">
            {{ log.ts }}
          </div>

          <div
            class="device-execution-log__dot"
            :class="{
              'device-execution-log__dot--cmd': log.type === 'cmd',
              'device-execution-log__dot--state': log.type === 'state',
              'device-execution-log__dot--resp': log.type === 'resp',
              'device-execution-log__dot--error': log.type === 'error',
            }"
          ></div>

          <div class="device-execution-log__content">
            <div v-if="log.actionId" class="device-execution-log__action-id">
              #{{ log.actionId }}
            </div>
            <div
              class="device-execution-log__message"
              :class="{
                'device-execution-log__message--cmd': log.type === 'cmd',
                'device-execution-log__message--state': log.type === 'state',
                'device-execution-log__message--resp': log.type === 'resp',
                'device-execution-log__message--error': log.type === 'error',
              }"
            >
              <span>{{ log.message }}</span>
              <span v-if="log.reason" class="device-execution-log__reason">
                (reason: {{ log.reason }})
              </span>
            </div>
          </div>
        </div>
      </template>

      <div v-if="logs.length === 0" class="device-execution-log__empty">
        No logs yet…
      </div>
    </div>
  </div>
</template>

<style scoped>
.device-execution-log {
  display: flex;
  flex-direction: column;
  user-select: none;
}

.device-execution-log__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.75rem;
  border-bottom: 1px solid var(--color-neutral-200);
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0.05em;
  text-transform: uppercase;
}

.device-execution-log__copy-button {
  position: relative;
}

.device-execution-log__copy-label {
  visibility: hidden;
}

.device-execution-log__copy-feedback {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}

.device-execution-log__body {
  padding: 0.5rem 1rem;
  color: var(--color-neutral-700);
  font-family: var(--font-mono);
  font-size: 0.6875rem;
  line-height: 1.25;
}

.device-execution-log__body > * + * {
  margin-top: 0.125rem;
}

.device-execution-log__action-heading {
  margin-top: 0.25rem;
  color: color-mix(in srgb, var(--color-blue-400) 80%, transparent);
  font-size: 0.625rem;
  letter-spacing: 0.025em;
  text-transform: uppercase;
}

.device-execution-log__row {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding: 1px 0.25rem;
  border-radius: var(--radius-sm);
  transition: background 150ms ease;
}

.device-execution-log__row:hover {
  background: var(--color-neutral-100);
}

.device-execution-log__time {
  width: 5rem;
  flex-shrink: 0;
  font-size: 0.625rem;
  opacity: 0.5;
  text-align: left;
}

.device-execution-log__dot {
  width: 0.5rem;
  height: 0.5rem;
  border-radius: 999px;
}

.device-execution-log__dot--cmd {
  background: var(--color-neutral-400);
}

.device-execution-log__dot--state {
  background: var(--color-blue-500);
}

.device-execution-log__dot--resp {
  background: var(--color-green-400);
}

.device-execution-log__dot--error {
  background: var(--color-red-500);
}

.device-execution-log__content {
  display: flex;
  flex: 1 1 auto;
  align-items: center;
  gap: 0.5rem;
  min-width: 0;
}

.device-execution-log__action-id {
  color: var(--color-blue-400);
  font-size: 0.625rem;
  font-weight: 600;
}

.device-execution-log__message {
  flex: 1 1 auto;
  min-width: 0;
  overflow-wrap: anywhere;
  white-space: pre-wrap;
}

.device-execution-log__message--cmd {
  color: var(--color-neutral-500);
}

.device-execution-log__message--state {
  color: var(--color-blue-400);
}

.device-execution-log__message--resp {
  color: var(--color-green-400);
}

.device-execution-log__message--error {
  color: var(--color-red-400);
}

.device-execution-log__reason {
  margin-left: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.device-execution-log__empty {
  padding: 0.5rem 0;
  font-style: italic;
  opacity: 0.4;
}

:global(.dark .device-execution-log__header) {
  border-bottom-color: var(--color-neutral-800);
  color: var(--color-neutral-400);
}

:global(.dark .device-execution-log__body) {
  color: var(--color-neutral-300);
}

:global(.dark .device-execution-log__row:hover) {
  background: color-mix(in srgb, var(--color-neutral-800) 50%, transparent);
}

:global(.dark .device-execution-log__message--cmd) {
  color: var(--color-neutral-300);
}

:global(.dark .device-execution-log__reason) {
  color: var(--color-neutral-400);
}

@media (min-width: 1024px) {
  .device-execution-log {
    height: 100%;
    min-height: 0;
  }

  .device-execution-log__body {
    flex: 1 1 auto;
    min-height: 0;
    overflow-y: auto;
  }
}
</style>
