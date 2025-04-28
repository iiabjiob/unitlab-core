<template>
  <div>
    <PageHeader title="Devices" />

    Hi there
  </div>
</template>

<script setup lang="ts">

import { onMounted, onUnmounted } from 'vue'
import { useWebSocketStore } from '@/stores/useWebsocketStore'
import { useDeviceStore } from '@/stores/useDeviceStore'

const wsStore = useWebSocketStore()
const deviceStore = useDeviceStore()

const topic = 'mqtt_device_register'

onMounted(() => {

  wsStore.subscribe([topic])
  deviceStore.requestScan();

})

// ✅ Отписка при размонтировании
onUnmounted(() => {

  wsStore.unsubscribe([topic])
})

</script>
