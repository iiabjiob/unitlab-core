<template>
  <UiToolbar title="Switchgears">
    <template #left>
      <UiButton type="toolbar" size="xs" @click="$emit('add')">➕ Add</UiButton>
      <UiButton type="toolbar" size="sm" @click="$emit('save')">💾 Save</UiButton>
      <UiButton type="toolbar" size="sm" @click="$emit('load')">📂 Import</UiButton>
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import UiButton from '../ui/UiButton.vue';
import UiToolbar from '../ui/UiToolbar.vue';

defineEmits<{
  (e: "add"): void
  (e: "save"): void
  (e: "load"): void
}>()
</script>
