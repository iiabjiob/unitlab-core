export interface Signal {
  index: number
  name: string
  state: boolean

  // Дополнительно:
  type?: 'DO' | 'DI' | 'AI' | 'AO'             // тип сигнала
  delayMs?: number                             // задержка перед активацией (групповая / индивидуальная)
  isPulse?: boolean                            // пульс или постоянный сигнал
  pulseDurationMs?: number                     // длительность пульса, если isPulse === true
}
