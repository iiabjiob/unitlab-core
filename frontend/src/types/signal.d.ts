export interface Signal {
  index: number
  name: string
  state: boolean | number

  type: 'DO' | 'DI' | 'AO'
  delayMs?: number                             // задержка перед активацией (групповая / индивидуальная)
  isPulse?: boolean                            // пульс или постоянный сигнал
  pulseDurationMs?: number                     // длительность пульса, если isPulse === true
}
