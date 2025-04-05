<template>
  <div>
    <PageHeader title="IO" />

    <div class="mt-6 space-y-8">
      <!-- Digital Outputs -->
      <div>
        <h2 class="text-lg font-semibold mb-2">Digital Outputs</h2>
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DoBoardComponent
            v-for="boardId in doBoardIds"
            :key="boardId"
            :board-name="boardId"
            :signals="getDoStatuses(boardId)"
          />
        </div>
      </div>

      <!-- Digital Inputs -->
      <div>
        <h2 class="text-lg font-semibold mb-2 border-t pt-4">Digital Inputs</h2>
        <div class="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-4">
          <DiBoardComponent
            v-for="boardId in diBoardIds"
            :key="boardId"
            :board-name="boardId"
            :signals="getDiStatuses(boardId)"
          />
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted, onUnmounted } from 'vue'
import PageHeader from '@/components/PageHeader.vue'
import DoBoardComponent from '@/components/DoBoardComponent.vue'
import DiBoardComponent from '@/components/DiBoardComponent.vue'
import { useWebSocketStore } from '@/stores/websocket'

const wsStore = useWebSocketStore()

// ❗ Задаются ID плат. Позже можно сделать динамическими.
const doBoardIds = ['do-board-1']
const diBoardIds = ['di-board-1']

onMounted(() => {
  const channels = [
    ...doBoardIds.map(id => `mqtt_do_board/${id}`),
    ...diBoardIds.map(id => `mqtt_di_board/${id}`)
  ]
  wsStore.subscribe(channels)
})

onUnmounted(() => {
  const channels = [
    ...doBoardIds.map(id => `mqtt_do_board/${id}`),
    ...diBoardIds.map(id => `mqtt_di_board/${id}`)
  ]
  wsStore.unsubscribe(channels)
})

function getDoStatuses(boardId) {
  return Object.entries(wsStore.receivedData)
    .filter(([key]) => key.startsWith(`${boardId}/status/`))
    .map(([key, value]) => ({
      name: key.split('/').pop(),
      state: value === 'true',
    }))
}

function getDiStatuses(boardId) {
  return Object.entries(wsStore.receivedData)
    .filter(([key]) => key.startsWith(`${boardId}/status/`))
    .map(([key, value]) => ({
      name: key.split('/').pop(),
      state: value === 'true',
    }))
}
</script>
