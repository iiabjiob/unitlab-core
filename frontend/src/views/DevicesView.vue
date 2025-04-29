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

import { WsTopicBuilder } from '@/utils/ws'

const wsStore = useWebSocketStore()
const deviceStore = useDeviceStore()

const topic_mqtt_device_register = WsTopicBuilder.mqttDeviceRegister()
const topic_device_registred = WsTopicBuilder.deviceRegistered()

const handleDeviceRegistered = (payload: any) => {
  deviceStore.upsertDevice({
    unit_id: payload.unit_id,
    type: payload.type ?? 'unknown',
    is_active: payload.is_active ?? true
  })
}

onMounted(() => {

  deviceStore.requestScan();
  wsStore.subscribe([topic_mqtt_device_register])
  wsStore.subscribeToChannel(topic_device_registred, handleDeviceRegistered)

})

onUnmounted(() => {
  wsStore.unsubscribe([topic_mqtt_device_register])
  wsStore.unsubscribeFromChannel(topic_device_registred, handleDeviceRegistered)
})

</script>
