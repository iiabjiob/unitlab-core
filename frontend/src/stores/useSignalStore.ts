import { defineStore } from 'pinia'
import { ref } from 'vue'
import { useWebSocketStore } from './useWebsocketStore'

import { TopicBuilder } from '@/utils/mqtt'

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

  function handleFullStateUpdate(unitId: string, outputs: Record<string, boolean>) {
    for (const [index, value] of Object.entries(outputs)) {
      const key = `${unitId}/${index}`
      setState(key, value)
    }
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

  // ✅ Публичные методы для компонентов:

  function requestStates(unitId: string) {
    wsStore.send({
      action: 'publish',
      topic: TopicBuilder.getStates(unitId),
      payload: {}
    })
  }

  function toggleSignal(unitId: string, signal: Signal, state: boolean) {
    const key = `${unitId}/${signal.index}`
    if (states.value[key] === state) return

    requestToggle(key, state)

    wsStore.send({
      action: 'publish',
      topic: TopicBuilder.set(signal.index, unitId),
      payload: { state }
    })
  }

  // function toggleAll(unitId: string, signals: Signal[], state: boolean, delay = 50, callback?: () => void) {
  //   const payload = buildGroupPayload(signals, state, delay)

  //   wsStore.send({
  //     action: 'publish',
  //     topic: TopicBuilder.group(unitId),
  //     payload
  //   })

  //   signals.forEach((signal, i) => {
  //     const key = `${unitId}/${signal.index}`
  //     setTimeout(() => requestToggle(key, state), i * delay)
  //   })

  //   if (callback) {
  //     const totalDelay = delay * (signals.length - 1) + 1000
  //     setTimeout(() => callback(), totalDelay)
  //   }
  // }

  function subscribeToUnitStates(unitId: string) {
    wsStore.subscribeToChannel(`${unitId}/states`, (outputs: Record<string, boolean>) => {
      handleFullStateUpdate(unitId, outputs)
    })
  }

  function subscribeToSignalUpdates(unitId: string, signals: Signal[]) {
    signals.forEach(signal => {
      const topic = `${unitId}/state/${signal.index}`
      wsStore.subscribeToChannel(topic, (value: boolean) => {
        handleMqttUpdate(`${unitId}/${signal.index}`, value)
      })
    })
  }

  return {
    states,
    pending,
    failed,
    setState,
    requestToggle,
    buildGroupPayload,
    requestStates,
    toggleSignal,
    // toggleAll,
    subscribeToUnitStates,
    subscribeToSignalUpdates,
  }
})
