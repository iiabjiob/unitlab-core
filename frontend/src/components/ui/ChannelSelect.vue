<template>
  <ChannelTreePicker
    :model-value="modelValue"
    :channel-type="channelType"
    :name="name"
    :exclude-ids="excludeIds"
    :disabled="disabled"
    @update:modelValue="updateValue"
  />
</template>

<script setup lang="ts">
import type { ChannelType } from "@/types/channel"
import ChannelTreePicker from "./ChannelTreePicker.vue"

const props = defineProps<{
  modelValue: number | null
  channelType: ChannelType
  name?: string
  excludeIds?: number[]
  disabled?: boolean
}>()
const emit = defineEmits<{
  (e: "update:modelValue", value: number | null): void
}>()

function updateValue(val: string | number | null) {
  if (val === null || val === "") {
    emit("update:modelValue", null)
    return
  }
  emit("update:modelValue", Number(val))
}
</script>
