<template>
  <div>
    <PageHeader title="IO" />

      <div class="space-y-5">

        <!-- Digital Outputs -->
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DoUnitComponent
            v-for="unitId in doUnitIds"
            :key="unitId"
            :unitId="unitId"
            :signals="getStatuses(unitId)"
          />
        </div>

        <!-- Digital Inputs -->
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DiUnitComponent
            v-for="unitId in diunitIds"
            :key="unitId"
            :unit-id="unitId"
            :signals="getStatuses(unitId)"
          />
        </div>
      </div>

  </div>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import PageHeader from '@/components/PageHeader.vue'
import DoUnitComponent from '@/components/DoUnitComponent.vue'
import DiUnitComponent from '@/components/DiUnitComponent.vue'
import { useWebSocketStore } from '@/stores/useWebsocketStore'

import { Signal } from '@/types/signal.d'

// TODO: ID плат (пока статично, позже будет динамика)
const doUnitIds: string[] = ['do-unit-58EC']
const diunitIds: string[] = ['di-unit-XXXX']

const wsStore = useWebSocketStore()

// ✅ Подписка на каналы при монтировании
onMounted(() => {
  const channels = [
    ...doUnitIds.map((id) => `mqtt_do_unit/${id}`),
    ...diunitIds.map((id) => `mqtt_di_unit/${id}`)
  ]
  wsStore.subscribe(channels)

})

// ✅ Отписка при размонтировании
onUnmounted(() => {
  const channels = [
    ...doUnitIds.map((id) => `mqtt_do_unit/${id}`),
    ...diunitIds.map((id) => `mqtt_di_unit/${id}`)
  ]
  wsStore.unsubscribe(channels)
})

// ✅ Получить статусы
function getStatuses(unitId: string): Signal[] {
  const raw = wsStore.receivedData[`${unitId}/state/group`]
  if (!raw) return []

  try {
    const parsed = JSON.parse(raw)
    const outputs: boolean[] = parsed.outputs ?? []

    return outputs.map((state, index) => ({
      index,
      name: `DO${index + 1}`,
      state
    }))
  } catch (e) {
    console.warn('❌ Failed to parse state/group payload:', raw)
    return []
  }
}

</script>
