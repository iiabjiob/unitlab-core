<template>
  <div>
    <!-- <PageHeader title="IO" /> -->

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
          :signals="signalStore.getSignals(unit.unit_id, unit.type)"
        />

        <DiUnitComponent
          v-for="unit in diUnits"
          :key="unit.unit_id"
          :unit-id="unit.unit_id"
          :signals="signalStore.getSignals(unit.unit_id, unit.type)"
        />
      </div>

      <!-- Digital Inputs -->
      <!-- <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
        <DiUnitComponent
          v-for="unit in diUnits"
          :key="unit.unit_id"
          :unit-id="unit.unit_id"
          :signals="signalStore.getSignals(unit.unit_id, unit.type)"
        />
      </div> -->
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
// const doUnits = computed(() => deviceStore.devices.filter(d => d.type === 'DO' && d.is_active))
// const diUnits = computed(() => deviceStore.devices.filter(d => d.type === 'DI' && d.is_active))

// TODO: заглушка для тестов
const doUnits = [
  { unit_id: 'dev-001', type: 'do', name: 'DO Unit 1' },
  { unit_id: 'dev-002', type: 'do', name: 'DO Unit 2' },
]
const diUnits = [
  { unit_id: 'dev-001', type: 'di', name: 'DI Unit 1' },
]
const aoUnits = [
  { unit_id: 'dev-003', type: 'ao', name: 'AO Unit 1' },
]

const noUnits = computed(() => doUnits.length === 0 && diUnits.length === 0)

onMounted(async() => {

  await deviceStore.fetchDevices()

  doUnits.forEach(unit => signalStore.subscribeToUnitStates(unit.unit_id, unit.type))
  diUnits.forEach(unit => signalStore.subscribeToUnitStates(unit.unit_id, unit.type))
  aoUnits.forEach(unit => signalStore.subscribeToUnitStates(unit.unit_id, unit.type))

  const allUnits = [...doUnits, ...diUnits]
  const channels = allUnits.map(u => WsTopicBuilder.unitStates(u.unit_id, u.type))

  // const allUnits = [...doUnits.value, ...diUnits.value]
  // const channels = allUnits.map(u => WsTopicBuilder.unitStates(u.unit_id, u.type))
  wsStore.subscribe(channels)

  // allUnits.forEach(unit => {
  //   signalStore.subscribeToUnitStates(unit.unit_id, unit.type)

  // })
})

onUnmounted(() => {

  doUnits.forEach(unit => signalStore.unsubscribeFromUnitStates(unit.unit_id, unit.type))
  diUnits.forEach(unit => signalStore.unsubscribeFromUnitStates(unit.unit_id, unit.type))
  aoUnits.forEach(unit => signalStore.unsubscribeFromUnitStates(unit.unit_id, unit.type))
  const allUnits = [...doUnits, ...diUnits]
  const channels = allUnits.map(u => WsTopicBuilder.unitStates(u.unit_id, u.type))
  wsStore.unsubscribe(channels)

  // allUnits.forEach(unit => {
  //   signalStore.unsubscribeFromUnitStates(unit.unit_id, unit.type)
  // })

})

</script>
