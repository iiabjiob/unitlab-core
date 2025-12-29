<script setup lang="ts">
import { computed, ref } from "vue"
import { useSequenceLogStore } from "@/stores/sequenceLogStore"
import type { SequenceDef, SequenceState } from "@/types/sequences"
import { useAutoScroll } from "@/composables/useAutoScroll"

const props = defineProps<{
  sequence: SequenceDef
  state: SequenceState
}>()

const logStore = useSequenceLogStore()
const logs = computed(() => logStore.logs[props.sequence.id] ?? [])

const logContainer = ref<HTMLElement | null>(null)
useAutoScroll(logs, logContainer)
</script>

<template>
  <div class="h-full flex flex-col select-none">

    <!-- HEADER -->
    <div class="p-4  text-xs uppercase tracking-wider 
                text-neutral-500 dark:text-neutral-400 border-b 
                border-neutral-200 dark:border-neutral-800">
      Execution Log
    </div>

    <!-- LIST -->
    <div
      ref="logContainer"
      class="flex-1 overflow-y-auto px-4 py-2 space-y-0.5
             font-mono text-[11px] leading-tight
             text-neutral-700 dark:text-neutral-300"
    >

      <!-- LOG ENTRY -->
      <div
        v-for="(log, i) in logs"
        :key="i"
        class="flex items-center gap-2 py-px px-1 rounded-sm
               hover:bg-neutral-100 dark:hover:bg-neutral-800/50 transition-colors"
      >

        <!-- TIMESTAMP -->
        <div class="text-[10px] opacity-50 w-20 shrink-0 text-left">
          {{ log.ts }}
        </div>

        <!-- DOT -->
        <div
          class="w-2 h-2 rounded-full"
          :class="{
            'bg-red-500': log.type === 'error',
            'bg-blue-400': log.type === 'step',
            'bg-neutral-400': log.type === 'info'
          }"
        ></div>

        <!-- MESSAGE -->
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

      <!-- EMPTY -->
      <div
        v-if="logs.length === 0"
        class="opacity-40 italic py-2"
      >
        No logs yet…
      </div>

    </div>
  </div>
</template>
