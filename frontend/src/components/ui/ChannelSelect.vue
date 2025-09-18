<template>
  <UiSelect
  :model-value="modelValue"
  :name="name"
  placeholder="— select channel —"
  @update:modelValue="val => $emit('update:modelValue', val ? Number(val) : null)"
>
  <option
    v-for="ch in filtered"
    :key="ch.id"
    :value="ch.id"
  >
    {{ channelStore.resolveUnitId(ch.device_id) }}/{{ ch.type.toUpperCase() }}{{ ch.index + 1 }} - {{ ch.name }}
  </option>
</UiSelect>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import type { ChannelType } from "@/types/channel";
import UiSelect from "./UiSelect.vue";

const props = defineProps<{
  modelValue: number | null
  channelType: ChannelType
  name?: string
  excludeIds?: number[]
}>()
const emit = defineEmits(["update:modelValue"])

const channelStore = useChannelStore()

const filtered = computed(() =>
  channelStore.channels.filter(ch => ch.type === props.channelType)
)
</script>
