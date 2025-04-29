export interface Device {
  unit_id: string
  type: string
  is_active: boolean
  isNew?: boolean

  // Дополнительно:
  type?: 'DO' | 'DI' | 'AI' | 'AO'             // тип сигнала
}
