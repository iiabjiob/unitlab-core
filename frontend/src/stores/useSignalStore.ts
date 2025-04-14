import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { useWebSocketStore } from './useWebsocketStore'

import type { Signal } from '@/types/signal'

type SignalKey = string // e.g., "dX-module-XXXX/<index>"
type SignalState = Record<SignalKey, boolean>
type PendingTimeouts = Record<SignalKey, ReturnType<typeof setTimeout>>
type FailedStates = Record<SignalKey, boolean>


export const useSignalStore = defineStore('signalStore', () => {
  const states = ref<SignalState>({})
  const pending = ref<PendingTimeouts>({})
  const failed = ref<FailedStates>({})

  const wsStore = useWebSocketStore()

  interface GroupAction {
    index: number
    state: boolean
    delay_ms: number
    is_pulse: boolean
    pulse_duration: number
  }

  interface GroupPayload {
    actions: GroupAction[]
  }

  // ✅ Listen to WebSocket updates and handle incoming DO state changes
  watch(
    () => wsStore.receivedData,
    (newData: Record<string, string>) => {
      for (const [channel, value] of Object.entries(newData)) {
        if (channel.includes('/status/')) {
          const key = channel.replace('/status/', '/')
          handleMqttUpdate(key, value === 'true')
        }
      }
    },
    { deep: true }
  )

  function setState(key: SignalKey, value: boolean) {
    states.value[key] = value
    if (pending.value[key]) {
      clearTimeout(pending.value[key])
      delete pending.value[key]
      delete failed.value[key]
    }
  }

  function handleMqttUpdate(key: SignalKey, value: boolean) {
    setState(key, value)
  }

  function requestToggle(key: SignalKey, value: boolean, timeout = 3000) {
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

  function buildGroupPayload(signals: Signal[], state: boolean, delay = 0): GroupPayload {
    return {
      actions: signals.map(signal => ({
        index: signal.index,
        state,
        delay_ms: signal.delayMs ?? delay,
        is_pulse: signal.isPulse ?? false,
        pulse_duration: signal.pulseDurationMs ?? 200 // по умолчанию 200мс
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
