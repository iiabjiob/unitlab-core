import { defineStore } from 'pinia'
import { ref } from 'vue'
import { useWebSocketStore } from './useWebsocketStore'
import { WsTopicBuilder } from '@/utils/ws'
import { Signal } from '@/types/signal'

type SignalKey = string  // e.g. "942c14/do/5" или "942c14/ao/2"
type SignalState = Record<SignalKey, boolean | number>

export const useSignalStore = defineStore('signalStore', () => {
  const wsStore = useWebSocketStore()

  // Сохраняем состояния по ключу `${unitId}/${deviceType}/${index}`
  const states = ref<SignalState>({})

  // Для DO и DI
  function handleDoDiState(payload: { unit_id: string, device_type: string, states: number[] }) {
    const { unit_id, device_type, states: bits } = payload
    bits.forEach((bit, index) => {
      const key = `${unit_id}/${device_type}/${index}`
      states.value[key] = Boolean(bit)
    })
  }

  // Для AO
  function handleAoState(payload: { unit_id: string, device_type: string, values: number[] }) {
    const { unit_id, device_type, values } = payload
    values.forEach((val, index) => {
      const key = `${unit_id}/${device_type}/${index}`
      states.value[key] = val
    })
  }

  // Универсальный обработчик (автоопределение типа)
  function handleUnitState(payload: any) {
    console.log(payload);

    if (payload.device_type === 'ao' && Array.isArray(payload.values)) {
      handleAoState(payload)
    } else if (['do', 'di'].includes(payload.device_type) && Array.isArray(payload.states)) {
      handleDoDiState(payload)
    }
    // можно добавить else — логирование ошибочного payload
  }

  // Подписка на канал состояния по unit_id + device_type
  function subscribeToUnitStates(unitId: string, deviceType: string) {
    const topic = WsTopicBuilder.unitStates(unitId, deviceType)
    wsStore.subscribeToChannel(topic, handleUnitState)
  }

  function unsubscribeFromUnitStates(unitId: string, deviceType: string) {
    const topic = WsTopicBuilder.unitStates(unitId, deviceType)
    wsStore.unsubscribeFromChannel(topic, handleUnitState)
  }

  // Получить все сигналы определенного типа и устройства (например, для таблицы)
  function getSignals(unitId: string, deviceType: string): Signal[] {
  const result: Signal[] = []
  const typeUpper = deviceType.toUpperCase() as Signal['type'] // 'DO', 'DI', 'AO', 'AI'
  for (const key in states.value) {
    if (key.startsWith(`${unitId}/${deviceType}/`)) {
      const parts = key.split('/')
      const index = parseInt(parts[2])
      result.push({
        index,
        name: `${typeUpper}${index + 1}`,   // Будет браться из сигнал листа
        state: states.value[key],
        type: typeUpper
      })
    }
  }
  result.sort((a, b) => a.index - b.index)
  return result
}

  return {
    states,
    subscribeToUnitStates,
    unsubscribeFromUnitStates,
    getSignals,
  }
})
