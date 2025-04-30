<template>
  <div>
    <PageHeader title="IO" />

    <!-- Индикатор загрузки -->
    <LoadingSpinner size="small" position="left" v-if="deviceStore.isLoading" />

    <div v-else-if="noUnits" class="mt-4">
      <AlertComponent type="warning" message="No active IO units found." />
    </div>

    <div v-else class="space-y-5">

      <!-- Digital Outputs -->
      <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
        <DoUnitComponent
          v-for="unit in doUnits"
          :key="unit.unit_id"
          :unitId="unit.unit_id"
          :signals="getStatuses(unit.unit_id)"
        />
      </div>

      <!-- Digital Inputs -->
      <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
        <DiUnitComponent
          v-for="unit in diUnits"
          :key="unit.unit_id"
          :unit-id="unit.unit_id"
          :signals="getStatuses(unit.unit_id)"
        />
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted, computed, ref } from 'vue'
import PageHeader from '@/components/PageHeader.vue'
import DoUnitComponent from '@/components/DoUnitComponent.vue'
import DiUnitComponent from '@/components/DiUnitComponent.vue'
import { useDeviceStore } from '@/stores/useDeviceStore'
import { useSignalStore } from '@/stores/useSignalStore'
import { useWebSocketStore } from '@/stores/useWebsocketStore'
import AlertComponent from '@/components/ui/AlertComponent.vue'
import LoadingSpinner from '@/components/LoadingSpinner.vue'

const deviceStore = useDeviceStore()
const signalStore = useSignalStore()
const wsStore     = useWebSocketStore()

// Группировка
const doUnits = computed(() => deviceStore.devices.filter(d => d.type === 'DO' && d.is_active))
const diUnits = computed(() => deviceStore.devices.filter(d => d.type === 'DI' && d.is_active))

const doChannels = doUnits.value.map(unit => `mqtt_unit_states/${unit.unit_id}`)
const diChannels = diUnits.value.map(unit => `mqtt_unit_states/${unit.unit_id}`)

const noUnits = computed(() => doUnits.value.length === 0 && diUnits.value.length === 0)

onMounted(() => {

  wsStore.subscribe([...doChannels, ... diChannels])

  if (!deviceStore.devices.length) {
    deviceStore.fetchDevices()
  }

  doUnits.value.forEach(unit => {
    // signalStore.requestStates(unit.unit_id)
    signalStore.subscribeToUnitStates(unit.unit_id)
    signalStore.subscribeToSignalUpdates(unit.unit_id, getStatuses(unit.unit_id))
  })
  diUnits.value.forEach(unit => {
    // signalStore.requestStates(unit.unit_id)
    signalStore.subscribeToUnitStates(unit.unit_id)
    signalStore.subscribeToSignalUpdates(unit.unit_id, getStatuses(unit.unit_id))
  })
})

onUnmounted(() => {
  wsStore.unsubscribe([...doChannels, ... diChannels])

  doUnits.value.forEach(unit => {
    signalStore.unsubscribeFromSignalUpdates(unit.unit_id, getStatuses(unit.unit_id))
    signalStore.unsubscribeFromUnitStates(unit.unit_id)
  })

  diUnits.value.forEach(unit => {
    signalStore.unsubscribeFromSignalUpdates(unit.unit_id, getStatuses(unit.unit_id))
    signalStore.unsubscribeFromUnitStates(unit.unit_id)
  })

})

function getStatuses(unitId: string) {
  const result: { index: number, name: string, state: boolean }[] = []

  for (const key in signalStore.states) {
    if (key.startsWith(`${unitId}/`)) {
      const index = parseInt(key.split('/')[1])
      result.push({
        index,
        name: `DO${index + 1}`,
        state: signalStore.states[key]
      })
    }
  }

  // отсортируем по индексу на всякий случай
  result.sort((a, b) => a.index - b.index)

  return result
}
</script>
