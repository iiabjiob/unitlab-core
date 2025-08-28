<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import { useWebSocketStore } from '@/stores/websocketStore'
import { useTimeStore } from '@/stores/timeStore'
import { WSChannel } from '@/types/ws/events'

const wsStore  = useWebSocketStore()
const timeStore = useTimeStore()

function handle(event: any) {
  timeStore.updateFromSync(event)
}

onMounted(() => {
  wsStore.subscribeToChannel(WSChannel.TIME_STATUS, handle)
})

onUnmounted(() => {
  wsStore.unsubscribeFromChannel(WSChannel.TIME_STATUS, handle)
})
</script>
