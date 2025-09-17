<template>
  <div class="text-xs text-neutral-600 dark:text-neutral-400 space-y-2">
    <!-- Control (DO) -->
    <div>
      <div class="font-medium text-neutral-700 dark:text-neutral-300 mb-1">
        Control
      </div>
      <div class="flex justify-between">
        <span>Open</span>
        <span v-if="doOpen">{{ doOpen.unitId }} [Ch {{ doOpen.channel }}]</span>
        <span v-else class="italic text-neutral-400">n/a</span>
      </div>
      <div class="flex justify-between">
        <span>Close</span>
        <span v-if="doClosed">{{ doClosed.unitId }} [Ch {{ doClosed.channel }}]</span>
        <span v-else class="italic text-neutral-400">n/a</span>
      </div>
    </div>
    <!-- Indication (DI) -->
    <div>
      <div class="font-medium text-neutral-700 dark:text-neutral-300 mb-1">
        Indication
      </div>
      <div class="flex justify-between">
        <span>Open</span>
        <span v-if="diOpen">{{ diOpen.unitId }} [Ch {{ diOpen.channel }}]</span>
        <span v-else class="italic text-neutral-400">n/a</span>
      </div>
      <div class="flex justify-between">
        <span>Close</span>
        <span v-if="diClose">{{ diClose.unitId }} [Ch {{ diClose.channel }}]</span>
        <span v-else class="italic text-neutral-400">n/a</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
const props = defineProps<{
  doOpen: { unitId: string; channel: number } | null
  doClosed: { unitId: string; channel: number } | null
  diOpen: { unitId: string; channel: number } | null
  diClose: { unitId: string; channel: number } | null
}>()
</script>
