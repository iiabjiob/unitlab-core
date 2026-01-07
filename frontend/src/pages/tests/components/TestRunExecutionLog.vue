<script setup lang="ts">
import { computed, ref } from "vue"
import type { TestRunState, TestRunSummary } from "@/types/testRuns"
import { useTestRunLogStore } from "@/stores/testRunLogStore"
import { useAutoScroll } from "@/composables/useAutoScroll"

const props = defineProps<{ run: TestRunSummary; state?: TestRunState | null }>()

const logStore = useTestRunLogStore()
const logs = computed(() => logStore.logs[props.run.id] ?? [])
const logContainer = ref<HTMLElement | null>(null)
useAutoScroll(logs, logContainer)
</script>

<template>
  <div class="h-full flex flex-col select-none">
    <div class="p-4 text-xs uppercase tracking-wider text-neutral-500 dark:text-neutral-400 border-b border-neutral-200 dark:border-neutral-800">
      Execution Log
    </div>
    <div
      ref="logContainer"
      class="flex-1 overflow-y-auto px-4 py-2 space-y-0.5 font-mono text-[11px] leading-tight text-neutral-700 dark:text-neutral-300"
    >
      <div
        v-for="(log, index) in logs"
        :key="index"
        class="flex items-center gap-2 py-px px-1 rounded-sm hover:bg-neutral-100 dark:hover:bg-neutral-800/50 transition-colors"
      >
        <div class="text-[10px] opacity-50 w-20 shrink-0 text-left">
          {{ log.ts }}
        </div>
        <div
          class="w-2 h-2 rounded-full"
          :class="{
            'bg-red-500': log.type === 'error',
            'bg-blue-400': log.type === 'step',
            'bg-neutral-400': log.type === 'info',
          }"
        ></div>
        <div
          class="whitespace-pre-wrap wrap-break-words flex-1"
          :class="{
            'text-red-400': log.type === 'error',
            'text-blue-300': log.type === 'step',
          }"
        >
          {{ log.message }}
        </div>
      </div>
      <div v-if="logs.length === 0" class="opacity-40 italic py-2">
        No logs yet…
      </div>
    </div>
  </div>
</template>
