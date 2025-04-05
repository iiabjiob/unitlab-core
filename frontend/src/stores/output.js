// stores/output.js
import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { useWebSocketStore } from './websocket'

export const useOutputStore = defineStore('output', () => {
  const states = ref({})      // key: 'do-board-1/do1' → true/false
  const pending = ref({})     // key: timeout ID (for rollback)
  const failed = ref({})      // key: true → failed confirmation

  const wsStore = useWebSocketStore()

  // ✅ Подписка на все изменения от WebSocket
  watch(
    () => wsStore.receivedData,
    (newData) => {
      for (const [channel, value] of Object.entries(newData)) {
        if (channel.startsWith('do-board-') && channel.includes('/status/')) {
          const key = channel.replace('/status/', '/')
          handleMqttUpdate(key, value === 'true')
        }
      }
    },
    { deep: true }
  )

  function setState(key, value) {
    states.value[key] = value
  }

  function handleMqttUpdate(key, value) {
    states.value[key] = value
    if (pending.value[key]) {
      clearTimeout(pending.value[key])
      delete pending.value[key]
      delete failed.value[key]  // reset error if success
    }
  }

  function requestToggle(key, value, timeout = 3000) {
    const previous = states.value[key]
    states.value[key] = value

    // 🔄 Очистим фейл, если была новая попытка
    delete failed.value[key]

    const id = setTimeout(() => {
      states.value[key] = previous
      failed.value[key] = true
      delete pending.value[key]
    }, timeout)

    pending.value[key] = id
  }

  function buildGroupPayload(signals, state, delay) {
    return {
      actions: signals.map(signal => ({
        index: parseInt(signal.name.replace(/\D/g, '')),
        state,
        delay_ms: delay  // единая задержка перед каждым
      }))
    }
  }

  return {
    states,
    pending,
    failed,
    setState,
    requestToggle,
    handleMqttUpdate,
    buildGroupPayload
  }
})
