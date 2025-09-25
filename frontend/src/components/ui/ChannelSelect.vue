<template>
  <UiSelect
    :model-value="modelValue?.index ?? null"
    :name="name"
    placeholder="— select channel —"
    @update:modelValue="val => {
      const channel = filtered.find(c => c.index === Number(val))
      if (channel) {
        $emit('update:modelValue', { unit_id: channelStore.resolveUnitId(channel.device_id), index: channel.index })
      }
    }"
  >
    <option
      v-for="ch in filtered"
      :key="ch.id"
      :value="ch.index"
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
  modelValue: { unit_id: string, index: number } | null
  channelType: ChannelType
  name?: string
  excludeIds?: number[]
}>()
const emit = defineEmits(["update:modelValue"])

const channelStore = useChannelStore()

const filtered = computed(() =>
  channelStore.channels.filter(ch =>
    ch.type === props.channelType &&
    !(props.excludeIds?.includes(ch.id))
  )
)
</script>
