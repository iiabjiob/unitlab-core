<template>
  <div>
    <PageHeader title="Devices" />

    Hi there
  </div>
</template>

<script setup lang="ts">

import { onMounted, onUnmounted } from 'vue'
import { useWebSocketStore } from '@/stores/useWebsocketStore'

const wsStore = useWebSocketStore()
const topic = 'mqtt_device_register'

onMounted(() => {

  wsStore.subscribe([topic])

})

// ✅ Отписка при размонтировании
onUnmounted(() => {

  wsStore.unsubscribe([topic])
})

</script>
