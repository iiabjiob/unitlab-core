<script setup lang="ts">
import { computed, onBeforeUnmount, ref } from "vue"
import { useChannelLogStore } from "@/stores/channelLogStore"
import type { Device } from "@/types/device";
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
  <div class="flex flex-col select-none lg:h-full lg:min-h-0">

    <div class="p-3 text-xs uppercase tracking-wider text-neutral-500 border-b
                dark:text-neutral-400 border-neutral-200 dark:border-neutral-800
                flex items-center justify-between gap-3">
      <span>Device Log</span>
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
      class="px-4 py-2 space-y-0.5 lg:flex-1 lg:min-h-0 lg:overflow-y-auto
             font-mono text-[11px] leading-tight
             text-neutral-700 dark:text-neutral-300"
    >

      <template v-for="(log, i) in logs" :key="i">
        <div
          v-if="log.actionId && (i === 0 || log.actionId !== logs[i - 1]?.actionId)"
          class="text-[10px] uppercase tracking-wide text-blue-400/80 mt-1"
        >
          Action {{ log.actionId }}
        </div>

        <div
          class="flex items-center gap-2 py-px px-1 rounded-sm
                 hover:bg-neutral-100 dark:hover:bg-neutral-800/50 transition-colors"
        >
          <div class="text-[10px] opacity-50 w-20 shrink-0 text-left">
            {{ log.ts }}
          </div>

          <div class="w-2 h-2 rounded-full"
               :class="{
                'bg-neutral-400': log.type === 'cmd',
                'bg-blue-500': log.type === 'state',
                'bg-green-500': log.type === 'resp',
                'bg-red-500': log.type === 'error',
               }"
          ></div>

          <div class="flex items-center gap-2 flex-1">
            <div v-if="log.actionId" class="text-[10px] text-blue-400 font-semibold">
              #{{ log.actionId }}
            </div>
            <div class="flex-1 whitespace-pre-wrap wrap-break-word"
                 :class="{
                   'text-neutral-500 dark:text-neutral-300': log.type === 'cmd',
                   'text-blue-400': log.type === 'state',
                   'text-green-400': log.type === 'resp',
                   'text-red-400': log.type === 'error',
                 }">
              <span>{{ log.message }}</span>
              <span v-if="log.reason" class="ml-1 text-xs text-neutral-500 dark:text-neutral-400">
                (reason: {{ log.reason }})
              </span>
            </div>
          </div>
        </div>
      </template>

      <div v-if="logs.length === 0"
           class="opacity-40 italic py-2">
        No logs yet…
      </div>
    </div>
  </div>
</template>
