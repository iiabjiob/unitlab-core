<template>
  <UiToolbar title="Devices">
    <template #left>
      <UiButton type="toolbar" size="sm">💾 Save</UiButton>
      <UiButton type="toolbar" size="sm">📂 Load</UiButton>
    </template>
    <template #right>

      <!-- Например, чекбокс "Только online" -->
      <label class="flex items-center gap-1 text-xs">
        <input type="checkbox" v-model="filterStore.onlyOnline" />
        Online only
      </label>

      <!-- Селект типов -->
      <!-- <select
        multiple
        class="text-xs border rounded px-1 py-0.5"
        v-model="filterStore.types"
      >
        <option value="di">Core</option>
        <option value="do">I/O</option>
        <option value="ao">Simulator</option>
      </select> -->
    </template>
  </UiToolbar>
</template>

<script setup lang="ts">
import UiToolbar from "@/components/ui/UiToolbar.vue"
import { useDeviceFilterStore } from "@/stores/deviceFilterStore"
import UiButton from "../ui/UiButton.vue";

const filterStore = useDeviceFilterStore()
</script>
