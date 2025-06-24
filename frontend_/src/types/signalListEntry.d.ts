export interface SignalListEntry {
  id: number | string                  // Уникальный идентификатор строки
  unit_id: string                      // unit_id устройства-источника
  channel: string                      // Имя/индекс физического канала (DO1, AO1, DI1...)
  terminal?: string                    // Терминал-приёмник (BCUF1, BCUD1, BCUG1)
  bay?: string                         // Bay (ячейка/панель/шкаф)
  signal_name: string                  // Имя сигнала (Slot F DI1 и т.д.)
  hmi_presentation_text?: string       // Для SCADA/HMI описание
  signal_type: 'DO' | 'DI' | 'AO' | 'AI'
  group?: string                       // Группа (CB1 и т.д.)
  reaction_matrix?: string             // Реакционная матрица
  external_address?: number            // Адрес во внешней системе (для автоматизации/SCADA)
  test_result?: 'pass' | 'fail'        // Результат теста
  tested_at?: string                   // Дата/время теста (ISO-строка)
}
