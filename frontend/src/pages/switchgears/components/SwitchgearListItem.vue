<script setup lang="ts">
import type { Switchgear } from "@/types/switchgear";

const props = defineProps<{
  swtichgear: Switchgear
  active: boolean
}>()

</script>

<template>
  <div
    class="group cursor-pointer select-none flex items-center px-3 py-2
           text-neutral-400 dark:text-neutral-600 hover:bg-neutral-850 transition-colors"
    :class="{
      'text-neutral-900 dark:text-white': active
    }"
  >
    <!-- Active indicator -->
    <div
      class="w-1 h-5 mr-2 rounded transition-colors"
      :class="active ? 'bg-blue-500' : 'bg-transparent group-hover:bg-neutral-700'"
    />

    <div class="flex items-center gap-2">
  
      <!-- SWITCHGEAR NAME -->
      <div class="truncate text-sm flex-1">
        {{ swtichgear.name }}
      </div>
  
      <!-- SWITCHGEAR TYPE -->
      <div class="text-xs truncate text-neutral-500 dark:text-neutral-400">
        {{ swtichgear.switchgear_type }}
      </div>
    </div>

  </div>
</template>
