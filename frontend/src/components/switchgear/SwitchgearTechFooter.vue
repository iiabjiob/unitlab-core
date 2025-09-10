<!-- src/components/switchgear/SwitchgearTechFooter.vue -->
<template>
  <div class="text-xs text-neutral-500 dark:text-neutral-400">
    <div>DO: {{ doUnitId }} [{{ doOpenCh }}|{{ doCloseCh }}]</div>
    <div>DI: {{ diUnitId }} [{{ diOpenPulseCh }}|{{ diClosePulseCh }}]</div>
  </div>
</template>

<script setup lang="ts">

const props = defineProps<{
  doUnitId: string
  doOpenCh: number
  doCloseCh: number
  diUnitId: string
  diOpenPulseCh: number
  diClosePulseCh: number
}>()

</script>
