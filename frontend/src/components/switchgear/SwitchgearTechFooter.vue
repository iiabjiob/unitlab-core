<!-- src/components/switchgear/SwitchgearTechFooter.vue -->
<template>
  <div class="text-xs text-neutral-500 dark:text-neutral-400 space-y-1">
    <div>
      DO:
      <span v-if="doOpen">Open → {{ doOpen.unitId }} [Ch {{ doOpen.channel }}]</span>
      <span v-else>Open → n/a</span>
      |
      <span v-if="doClosed">Closed → {{ doClosed.unitId }} [Ch {{ doClosed.channel }}]</span>
      <span v-else>Closed → n/a</span>
    </div>

    <div>
      DI:
      <span v-if="diOpen">Open → {{ diOpen.unitId }} [Ch {{ diOpen.channel }}]</span>
      <span v-else>Open → n/a</span>
      |
      <span v-if="diClose">Close → {{ diClose.unitId }} [Ch {{ diClose.channel }}]</span>
      <span v-else>Close → n/a</span>
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
