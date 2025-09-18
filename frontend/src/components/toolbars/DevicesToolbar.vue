<template>
  <UiToolbar title="Devices">
    <template #left>
      <UiButton type="toolbar" size="sm">💾 Save</UiButton>
      <UiButton type="toolbar" size="sm">📂 Load</UiButton>
    </template>
    <template #right>
      <DeviceFilterMenu />
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import UiToolbar from "@/components/ui/UiToolbar.vue"
import UiButton from "../ui/UiButton.vue";
import DeviceFilterMenu from "../devices/DeviceFilterMenu.vue";

</script>
