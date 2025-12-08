<template>
  <div
    class="group flex items-center justify-between px-2 py-1.5 text-sm select-none
           transition-colors
           hover:bg-neutral-100 dark:hover:bg-neutral-800"
  >
    <!-- LEFT -->
    <div class="flex items-center gap-3">

      <!-- Status dot -->
      <div class="w-2 h-2 rounded-full"/>

      
      <!-- Description -->
      <div class="text-xs text-neutral-700 dark:text-neutral-300">
        {{ channel.resolved_name }}
      </div>
    </div>

  </div>
</template>



<script setup lang="ts">
import { computed } from "vue"
import type { Channel } from "@/types/channel"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"

const props = defineProps<{
  channel: Channel
}>()

const deviceStore = useDeviceStore()
const channelStore = useChannelStore()

</script>
