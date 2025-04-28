import { defineStore } from 'pinia'
import { useWebSocketStore } from './useWebsocketStore'

import { TopicBuilder } from '@/utils/topics'


export const useDeviceStore = defineStore('deviceStore', () => {


  const wsStore = useWebSocketStore()


  // ✅ Публичные методы для компонентов:

  function requestScan() {
    wsStore.send({
      action: 'publish',
      topic: TopicBuilder.deviceScan(),
      payload: {}
    })
  }

  return {
    requestScan,
  }

})
