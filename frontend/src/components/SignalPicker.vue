<template>
  <select
    class="w-full text-xs px-1 py-0.5 bg-white dark:bg-neutral-900 border rounded"
    :value="selectedValue"
    @change="onChange($event)"
  >
    <option disabled value="">Select signal…</option>
    <optgroup
      v-for="device in filteredDevices"
      :key="device.unit_id"
      :label="device.name || device.unit_id"
    >
      <option
        v-for="(ch, idx) in device.num_channels"
        :key="idx"
        :value="`${device.unit_id}:${idx}`"
      >
        {{ device.unit_id }} / Ch {{ idx }} –
      </option>
    </optgroup>
  </select>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"

const props = defineProps<{
  kind: "DI" | "DO"
  modelUnitId: string | null
  modelChannel: number | null
}>()

const emit = defineEmits<{
  (e: "update", value: { unitId: string; channel: number }): void
}>()

const deviceStore = useDeviceStore()

const filteredDevices = computed(() =>
  deviceStore.devices.filter(d => d.type === props.kind)
)

const selectedValue = computed(() =>
  props.modelUnitId && props.modelChannel !== null
    ? `${props.modelUnitId}:${props.modelChannel}`
    : ""
)

function onChange(e: Event) {
  const val = (e.target as HTMLSelectElement).value
  const [unitId, chStr] = val.split(":")
  emit("update", { unitId, channel: Number(chStr) })
}
</script>
