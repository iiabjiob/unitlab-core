export interface Signal {
  id: string | number        // Уникальный идентификатор сигнала/канала
  device_id: string          // Ссылка на unit_id устройства
  type: 'DO' | 'DI' | 'AO'   // Тип канала (цифровой/аналоговый, in/out)
  index: number              // Порядковый номер внутри устройства (0..N)
  name?: string              // Имя/лейбл канала для UI
  state: boolean | number    // Текущее состояние (bool для DO/DI, number для AO/AI)
  last_updated?: string      // Последнее обновление (ISO8601)
  is_allocated?: boolean     // Привязан ли к сигнал-листу (для UI)
  // DO-specific:
  delayBeforeMs?: number
  isPulse?: boolean
  pulseDurationMs?: number
  // AO-specific:
  minValue?: number
  maxValue?: number
  // Для DI/AI можно добавить threshold/scale при необходимости
}
