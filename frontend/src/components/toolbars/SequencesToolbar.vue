<template>
  <UiToolbar title="Sequences">
    <template #left>
      <UiButton type="toolbar" size="sm" @click="$emit('add')">➕ Add</UiButton>
      <UiButton
        type="toolbar"
        size="sm"
        :disabled="importing"
        @click="$emit('import')"
      >
        <span v-if="importing">⏳ Importing…</span>
        <span v-else>📂 Import</span>
      </UiButton>
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import UiButton from "../ui/UiButton.vue"
import UiToolbar from "../ui/UiToolbar.vue"

defineProps<{ importing: boolean }>()
defineEmits<{ (e: "add"): void; (e: "import"): void }>()
</script>
