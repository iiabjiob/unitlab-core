<template>
  <div>
    <div class="flex items-center justify-between mb-3">
      <ButtonComponent
        type="secondary"
        size="sm"
        @click="toggleSelectAll"
      >
        {{ allSelected ? 'Deselect all' : 'Select all' }}
      </ButtonComponent>

      <ButtonComponent
        type="danger"
        size="sm"
        :disabled="!selected.length"
        @click="$emit('delete-multiple', selected)"
      >
        Delete selected ({{ selected.length }})
      </ButtonComponent>
    </div>

    <div class="flex flex-col gap-3">
      <DeviceRow
        v-for="device in devices"
        :key="device.unit_id"
        :device="device"
        :selected="selected.includes(device.unit_id)"
        @toggle="$emit('toggle', device.unit_id)"
        @delete="$emit('delete', device.unit_id)"
        @select="toggleSelection"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, watch, computed } from 'vue'
import type { Device } from '@/types/device'
import DeviceRow from './DeviceRow.vue'
import ButtonComponent from './ui/ButtonComponent.vue'

const props = defineProps<{
  devices: Device[]
}>()

const emit = defineEmits<{
  (e: 'toggle', unitId: string): void
  (e: 'delete', unitId: string): void
  (e: 'delete-multiple', unitIds: string[]): void
}>()

const selected = ref<string[]>([])

function toggleSelection(unitId: string) {
  if (selected.value.includes(unitId)) {
    selected.value = selected.value.filter(id => id !== unitId)
  } else {
    selected.value.push(unitId)
  }
}

const allSelected = computed(() => selected.value.length === props.devices.length)

function toggleSelectAll() {
  if (allSelected.value) {
    selected.value = []
  } else {
    selected.value = props.devices.map(d => d.unit_id)
  }
}

watch(() => props.devices.length, () => {
  selected.value = []
})
</script>
