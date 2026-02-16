// src/constants/switchgear.ts
export const SWITCHGEAR_CODE = {
  // 00
  UNKNOWN: 0b00,
  // 01
  OPEN: 0b01,
  // 10
  CLOSED: 0b10,
  // 11
  INTERMEDIATE: 0b11,
} as const

export type SwitchgearState = keyof typeof SWITCHGEAR_CODE

export const SWITCHGEAR_OPTIONS = Object.keys(SWITCHGEAR_CODE) as SwitchgearState[]

export function codeToState(open: boolean, closed: boolean): SwitchgearState {
  const code = ((open ? 1 : 0) << 0) | ((closed ? 1 : 0) << 1)
  const entry = Object.entries(SWITCHGEAR_CODE).find(([_, v]) => v === code)
  return (entry?.[0] as SwitchgearState) ?? "UNKNOWN"
}
