<template>
  <div
    class="group flex items-center justify-between px-2 py-1.5 text-sm select-none"
  >
    <div class="flex items-center gap-3">

      <!-- DO control -->
      <div
        v-if="channel.type === 'do'"
        class="w-4 h-4 rounded-sm border cursor-pointer flex items-center justify-center
               transition-colors"
        :class="{
          'bg-green-500 border-green-600': channel.state,
          'bg-neutral-300 dark:bg-neutral-700 border-neutral-600 hover:bg-neutral-600': !channel.state
        }"
        @click.stop="emit('toggle', channel)"
      >
        <span v-if="channel.state" class="text-[10px] text-white">✓</span>
      </div>

      <!-- DI indicator -->
      <div
        v-else-if="channel.type === 'di'"
        class="w-4 h-4 rounded-full border"
        :class="{
          'bg-green-500 border-green-600': channel.state,
          'bg-neutral-600 border-neutral-500': !channel.state
        }"
      />

      <!-- AO input -->
      <input
        v-else-if="channel.type === 'ao'"
        type="number"
        class="w-16 px-1 py-0.5 text-xs rounded border border-neutral-600
               bg-neutral-900 text-neutral-200"
        :value="channel.state"
        @change="onAoChange"
      />

      <!-- Label -->
      <div class="text-xs text-neutral-700 dark:text-neutral-300">
        {{ channel.resolved_name }}
      </div>
    </div>
  </div>
</template>


<script setup lang="ts">
import type { Channel } from "@/types/channel"

const props = defineProps<{ channel: Channel }>()

const emit = defineEmits<{
  (e: "toggle", ch: Channel): void
  (e: "set-ao", payload: { channel: Channel; value: number }): void
}>()

function onAoChange(event: Event) {
  const raw = (event.target as HTMLInputElement).value
  const num = Number(raw)
  if (!isNaN(num)) {
    emit("set-ao", { channel: props.channel, value: num })
  }
}
</script>
