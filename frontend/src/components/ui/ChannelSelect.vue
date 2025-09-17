<!-- src/components/ui/ChannelSelect.vue -->
<template>
  <div class="w-full">

    <select
      class="w-full px-2 py-0.5 text-xs border rounded-sm border-neutral-300 dark:border-neutral-700 bg-white dark:bg-neutral-800"
      :value="modelValue ?? ''"
      :name="name"
      @change="e => $emit('update:modelValue', (e.target as HTMLSelectElement).value ? Number((e.target as HTMLSelectElement).value) : null)"

    >
      <option value="">— not set —</option>
      <option
        v-for="ch in filtered"
        :key="ch.id"
        :value="ch.id"
      >
        {{ channelStore.resolveUnitId(ch.device_id) }}/{{ ch.type.toUpperCase() }}{{ ch.index + 1 }} - {{ ch.name }}
      </option>
    </select>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"

const props = defineProps<{
  modelValue: number | null
  channelType: "do" | "di"
  name?: string
  excludeIds?: number[]
}>()
const emit = defineEmits(["update:modelValue"])

const channelStore = useChannelStore()

const filtered = computed(() =>
  channelStore.channels.filter(
    ch => ch.type === props.channelType
  )
)

</script>
