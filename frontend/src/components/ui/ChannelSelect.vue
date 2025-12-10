<template>
  <UiSelect
    :model-value="modelValue"
    :name="name"
    placeholder="— select channel —"
    :disabled="disabled"
    @update:modelValue="updateValue"
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
  disabled?: boolean
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

function updateValue(val: string | number | null) {
  if (val === null || val === "") {
    emit("update:modelValue", null)
    return
  }
  emit("update:modelValue", Number(val))
}
</script>
