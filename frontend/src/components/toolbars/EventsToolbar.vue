<template>
  <UiToolbar title="Events">
    <template #left>

      <UiButton type="toolbar" size="sm" @click="onExport">📤 Export</UiButton>
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import UiButton from '../ui/UiButton.vue';
import UiToolbar from '../ui/UiToolbar.vue';

async function onExport() {

  console.log("Dummy export to excel")
}

</script>
