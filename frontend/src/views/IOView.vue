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
          :signals="signalStore.getUnitSignals(unit.unit_id)"
        />
      </div>

      <!-- Digital Inputs -->
      <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
        <DiUnitComponent
          v-for="unit in diUnits"
          :key="unit.unit_id"
          :unit-id="unit.unit_id"
          :signals="signalStore.getUnitSignals(unit.unit_id)"
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
import { WsTopicBuilder } from '@/utils/ws'

const deviceStore = useDeviceStore()
const signalStore = useSignalStore()
const wsStore     = useWebSocketStore()

// Группировка
const doUnits = computed(() => deviceStore.devices.filter(d => d.type === 'DO' && d.is_active))
const diUnits = computed(() => deviceStore.devices.filter(d => d.type === 'DI' && d.is_active))

const noUnits = computed(() => doUnits.value.length === 0 && diUnits.value.length === 0)

onMounted(async() => {

  await deviceStore.fetchDevices()

  const allUnits = [...doUnits.value, ...diUnits.value]
  const channels = allUnits.map(u => WsTopicBuilder.unitStates(u.unit_id))
  wsStore.subscribe(channels)

  allUnits.forEach(unit => {
    signalStore.subscribeToUnitStates(unit.unit_id)
    signalStore.subscribeToSignalUpdates(unit.unit_id, signalStore.getUnitSignals(unit.unit_id))

    signalStore.requestStates(unit.unit_id)
  })
})

onUnmounted(() => {
  const allUnits = [...doUnits.value, ...diUnits.value]
  const channels = allUnits.map(u => WsTopicBuilder.unitStates(u.unit_id))
  wsStore.unsubscribe(channels)

  allUnits.forEach(unit => {
    signalStore.unsubscribeFromSignalUpdates(unit.unit_id, signalStore.getUnitSignals(unit.unit_id))
    signalStore.unsubscribeFromUnitStates(unit.unit_id)
  })

})

</script>
