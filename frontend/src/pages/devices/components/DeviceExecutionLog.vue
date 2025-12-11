<script setup lang="ts">
import { computed } from "vue"
import { useChannelLogStore } from "@/stores/channelLogStore"
import type { Device } from "@/types/device";

const props = defineProps<{ device: Device }>()

const logStore = useChannelLogStore()
const logs = computed(() => logStore.logs[props.device.id] ?? [])
const sortedLogs = computed(() =>
  [...(logStore.logs[props.device.id] ?? [])]
    .sort((a, b) => a.t - b.t)
)
</script>

<template>
  <div class="h-full flex flex-col select-none">

    <div class="p-3 text-xs uppercase tracking-wider text-neutral-500 border-b
                dark:text-neutral-400 border-neutral-200 dark:border-neutral-800">
      Device Log
    </div>

    <div
      class="flex-1 overflow-y-auto px-4 py-2 space-y-0.5
             font-mono text-[11px] leading-tight
             text-neutral-700 dark:text-neutral-300"
    >

      <div
        v-for="(log, i) in sortedLogs"
        :key="i"
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

        <div class="flex-1 whitespace-pre-wrap wrap-break-word"
             :class="{
              //  'text-neutral-400': log.type === 'cmd',
               'text-blue-400': log.type === 'state',
               'text-green-400': log.type === 'resp',
               'text-red-400': log.type === 'error',
             }">
          {{ log.message }}
        </div>
      </div>

      <div v-if="logs.length === 0"
           class="opacity-40 italic py-2">
        No logs yet…
      </div>
    </div>
  </div>
</template>
