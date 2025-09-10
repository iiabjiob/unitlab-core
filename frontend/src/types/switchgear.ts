export interface Switchgear {
  id: string
  kind: "switchgear"
  title: string

  // Управляющие выходы (DO → устройство)
  doOpen: { unitId: string; channel: number } | null
  doClosed: { unitId: string; channel: number } | null

  // Обратная связь (DI ← устройство)
  diOpen: { unitId: string; channel: number } | null
  diClose: { unitId: string; channel: number } | null

  // Задержка подтверждения
  feedbackDelayMs: number
}
