<template>
  <div>
    <PageHeader title="IO" />

      <div class="space-y-5">

        <!-- Digital Outputs -->
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DoBoardComponent
            v-for="boardId in doBoardIds"
            :key="boardId"
            :board-name="boardId"
            :signals="getStatuses(boardId)"
          />
        </div>

        <!-- Digital Inputs -->
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DiBoardComponent
            v-for="boardId in diBoardIds"
            :key="boardId"
            :board-name="boardId"
            :signals="getStatuses(boardId)"
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
import { useWebSocketStore } from '@/stores/websocket'

import { Signal } from '@/types/signal'

// TODO: ID плат (пока статично, позже будет динамика)
const doBoardIds: string[] = ['do-unit-58EC']
const diBoardIds: string[] = ['di-board-XXXX']

const wsStore = useWebSocketStore()

// ✅ Подписка на каналы при монтировании
onMounted(() => {
  const channels = [
    ...doBoardIds.map((id) => `mqtt_do_board/${id}`),
    ...diBoardIds.map((id) => `mqtt_di_board/${id}`)
  ]
  wsStore.subscribe(channels)
})

// ✅ Отписка при размонтировании
onUnmounted(() => {
  const channels = [
    ...doBoardIds.map((id) => `mqtt_do_board/${id}`),
    ...diBoardIds.map((id) => `mqtt_di_board/${id}`)
  ]
  wsStore.unsubscribe(channels)
})

// ✅ Получить статусы
function getStatuses(boardId: string): Signal[] {
  return Object.entries(wsStore.receivedData)
    .filter(([key]) => key.startsWith(`${boardId}/status/`))
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
