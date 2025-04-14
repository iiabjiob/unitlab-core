<template>
  <div>
    <PageHeader title="IO" />

      <div class="space-y-5">

        <!-- Digital Outputs -->
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DoBoardComponent
            v-for="unitId in doUnitIds"
            :key="unitId"
            :unitId="unitId"
            :signals="getStatuses(unitId)"
          />
        </div>

        <!-- Digital Inputs -->
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DiBoardComponent
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
import DoBoardComponent from '@/components/DoBoardComponent.vue'
import DiBoardComponent from '@/components/DiBoardComponent.vue'
import { useWebSocketStore } from '@/stores/useWebsocketStore'

import { Signal } from '@/types/signal'

// TODO: ID плат (пока статично, позже будет динамика)
const doUnitIds: string[] = ['do-unit-58EC']
const diunitIds: string[] = ['di-board-XXXX']

const wsStore = useWebSocketStore()

// ✅ Подписка на каналы при монтировании
onMounted(() => {
  const channels = [
    ...doUnitIds.map((id) => `mqtt_do_board/${id}`),
    ...diunitIds.map((id) => `mqtt_di_board/${id}`)
  ]
  wsStore.subscribe(channels)
})

// ✅ Отписка при размонтировании
onUnmounted(() => {
  const channels = [
    ...doUnitIds.map((id) => `mqtt_do_board/${id}`),
    ...diunitIds.map((id) => `mqtt_di_board/${id}`)
  ]
  wsStore.unsubscribe(channels)
})

// ✅ Получить статусы
function getStatuses(unitId: string): Signal[] {
  return Object.entries(wsStore.receivedData)
    .filter(([key]) => key.startsWith(`${unitId}/status/`))
    .map(([key, value]) => {
      const index = parseInt(key.split('/').pop() || '')
      return {
        index,
        name: `DO${index + 1}`, // пока временное имя, можно заменить позже
        state: value === 'true'
      }
    })
}

</script>
