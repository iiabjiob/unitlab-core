import { defineStore } from 'pinia'
import { ref } from 'vue'
import { useWebSocketStore } from './useWebsocketStore'

import type { Signal } from '@/types/signal'
import { WsTopicBuilder } from '@/utils/ws'
import { buildSetPinCommand } from '@/utils/buildSetPinCommand'

type SignalKey       = string // e.g., "dX-module-XXXX/<index>"
type SignalState     = Record<SignalKey, boolean>
type PendingTimeouts = Record<SignalKey, ReturnType<typeof setTimeout>>
type FailedStates    = Record<SignalKey, boolean>


export const useSignalStore = defineStore('signalStore', () => {
  const wsStore = useWebSocketStore()

  const states  = ref<SignalState>({})
  const pending = ref<PendingTimeouts>({})
  const failed  = ref<FailedStates>({})

  function setState(key: SignalKey, value: boolean) {
    states.value[key] = value
    if (pending.value[key]) {
      clearTimeout(pending.value[key])
      delete pending.value[key]
    }
    delete failed.value[key]
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

    // Очистить прошлые таймеры, если были
    if (pending.value[key]) {
      clearTimeout(pending.value[key])
      delete pending.value[key]
    }

    // Ставим таймер ожидания подтверждения
    const confirmTimeout = setTimeout(() => {
      // Подтверждение не пришло — возвращаем предыдущее состояние
      states.value[key] = previous
      failed.value[key] = true

      // Через 2 секунды убираем крест (ошибку)
      setTimeout(() => {
        delete failed.value[key]
      }, 2000)

      // Удаляем из pending (чтобы isWaiting стал false)
      delete pending.value[key]
    }, timeout)

    // Сохраняем таймер ожидания
    pending.value[key] = confirmTimeout
  }

  // ✅ Публичные методы для компонентов:

  function requestStates(unitId: string) {
    wsStore.send({
      action: 'request_states',
      unitId,
    })
  }

  function toggleSignal(unitId: string, signal: Signal, state: boolean) {
    const key = `${unitId}/${signal.index}`
    if (states.value[key] === state) return

    requestToggle(key, state)

    const command = buildSetPinCommand(signal, unitId, state)
    wsStore.send(command)

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

  const unitStateHandlers = new Map<string, (payload: any) => void>()
  const signalHandlers = new Map<string, (value: boolean) => void>() // key: `${unitId}/state/${index}`

  function subscribeToUnitStates(unitId: string) {
    const topic = WsTopicBuilder.unitStates(unitId)
    const handler = (outputs: Record<string, boolean>) => {
      handleFullStateUpdate(unitId, outputs)
    }
    unitStateHandlers.set(topic, handler)
    wsStore.subscribeToChannel(topic, handler)
  }

  function unsubscribeFromUnitStates(unitId: string) {
    const topic = WsTopicBuilder.unitStates(unitId)
    const handler = unitStateHandlers.get(topic)
    if (handler) {
      wsStore.unsubscribeFromChannel(topic, handler)
      unitStateHandlers.delete(topic)
    }
  }

  function subscribeToSignalUpdates(unitId: string, signals: Signal[]) {
    signals.forEach(signal => {

      const topic = `${unitId}/state/${signal.index}`
      const key = `${unitId}/${signal.index}`

      const handler = (value: boolean) => handleMqttUpdate(key, value)
      signalHandlers.set(topic, handler)
      wsStore.subscribeToChannel(topic, handler)
    })
  }

  function unsubscribeFromSignalUpdates(unitId: string, signals: Signal[]) {
    signals.forEach(signal => {
      const topic = `${unitId}/state/${signal.index}`
      const handler = signalHandlers.get(topic)
      if (handler) {
        wsStore.unsubscribeFromChannel(topic, handler)
        signalHandlers.delete(topic)
      }
    })
  }

  function getUnitSignals(unitId: string): { index: number; name: string; state: boolean }[] {
    const result: { index: number; name: string; state: boolean }[] = []
    const type = unitId.split('-')[0].toUpperCase() || 'IO'

    for (const key in states.value) {
      if (key.startsWith(`${unitId}/`)) {
        const index = parseInt(key.split('/')[1])
        result.push({
          index,
          name: `${type}${index + 1}`,
          state: states.value[key]
        })
      }
    }

    result.sort((a, b) => a.index - b.index)
    return result
  }


  return {
    states,
    pending,
    failed,
    toggleSignal,
    setState,
    requestToggle,
    requestStates,
    // toggleAll,
    getUnitSignals,
    subscribeToUnitStates,
    subscribeToSignalUpdates,
    unsubscribeFromUnitStates,
    unsubscribeFromSignalUpdates,
  }
})
