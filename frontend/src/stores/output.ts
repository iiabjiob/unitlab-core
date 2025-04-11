import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { useWebSocketStore } from './websocket'

type OutputKey = string // e.g., "do-board-1/do1"
type OutputState = Record<OutputKey, boolean>
type PendingTimeouts = Record<OutputKey, ReturnType<typeof setTimeout>>
type FailedStates = Record<OutputKey, boolean>

interface GroupAction {
  index: number
  state: boolean
  delay_ms: number
}

interface GroupPayload {
  actions: GroupAction[]
}

export const useOutputStore = defineStore('output', () => {
  const states = ref<OutputState>({})
  const pending = ref<PendingTimeouts>({})
  const failed = ref<FailedStates>({})

  const wsStore = useWebSocketStore()

  // ✅ Listen to WebSocket updates and handle incoming DO state changes
  watch(
    () => wsStore.receivedData,
    (newData: Record<string, string>) => {
      for (const [channel, value] of Object.entries(newData)) {
        if (channel.startsWith('do-board-') && channel.includes('/status/')) {
          const key = channel.replace('/status/', '/')
          handleMqttUpdate(key, value === 'true')
        }
      }
    },
    { deep: true }
  )

  function setState(key: OutputKey, value: boolean) {
    states.value[key] = value
  }

  function handleMqttUpdate(key: OutputKey, value: boolean) {
    states.value[key] = value
    if (pending.value[key]) {
      clearTimeout(pending.value[key])
      delete pending.value[key]
      delete failed.value[key]
    }
  }

  function requestToggle(key: OutputKey, value: boolean, timeout = 3000) {
    const previous = states.value[key]
    states.value[key] = value

    // Clear any previous failure status
    delete failed.value[key]

    const id = setTimeout(() => {
      states.value[key] = previous
      failed.value[key] = true
      delete pending.value[key]
    }, timeout)

    pending.value[key] = id
  }

  function buildGroupPayload(signals: { name: string }[], state: boolean, delay: number): GroupPayload {
    return {
      actions: signals.map(signal => ({
        index: parseInt(signal.name.replace(/\D/g, '')),
        state,
        delay_ms: delay
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
