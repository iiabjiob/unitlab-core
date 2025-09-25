<template>
  <UiSelect
    :model-value="modelValue"
    :name="name"
    placeholder="— select channel —"
    @update:modelValue="val => $emit('update:modelValue', val === null ? null : Number(val))"
  >
    <option
      v-for="ch in filtered"
      :key="ch.id"
      :value="ch.id"
    >
      {{ channelStore.resolveChannelFullLabel(ch) }}
    </option>
  </UiSelect>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import type { ChannelType } from "@/types/channel"
import UiSelect from "./UiSelect.vue"

const props = defineProps<{
  modelValue: number | null
  channelType: ChannelType
  name?: string
  excludeIds?: number[]
}>()
const emit = defineEmits<{
  (e: "update:modelValue", value: number | null): void
}>()

const channelStore = useChannelStore()

const filtered = computed(() =>
  channelStore.channels.filter(
    ch =>
      ch.type === props.channelType &&
      !(props.excludeIds?.includes(ch.id))
  )
)
</script>
