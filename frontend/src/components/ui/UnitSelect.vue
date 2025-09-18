<template>
  <UiSelect
  :model-value="modelValue"
  :name="name"
  placeholder="— select unit —"
  @update:modelValue="val => $emit('update:modelValue', val)"
>
  <option
    v-for="unit in units"
    :key="unit.id"
    :value="unit.id"
  >
    {{ unit.name ?? unit.id }}
  </option>
</UiSelect>

</template>

<script setup lang="ts">
import { computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import UiSelect from "./UiSelect.vue";

const props = defineProps<{
  modelValue: string | null
  name?: string
}>()
const emit = defineEmits(["update:modelValue"])

const deviceStore = useDeviceStore()
const units = computed(() => deviceStore.devices)
</script>
