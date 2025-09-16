<template>
  <div class="h-dvh flex bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200">
    <SequencePalette />
    <SequenceWorkspace />
  </div>
</template>

<script setup lang="ts">
import SequencePalette from '@/components/editor/SequencePalette.vue';
import SequenceWorkspace from '@/components/editor/SequenceWorkspace.vue';

</script>
