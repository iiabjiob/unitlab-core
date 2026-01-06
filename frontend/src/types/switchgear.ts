export type SwitchgearType = "switchgear" | "disconnector" | "earthing" | (string & {})

export type SwitchgearBindingRole =
  | "do_open"
  | "do_closed"
  | "di_open"
  | "di_close"
  | (string & {})

export const SWITCHGEAR_BINDING_ROLES: SwitchgearBindingRole[] = [
  "do_open",
  "do_closed",
  "di_open",
  "di_close",
]

export interface SwitchgearBinding {
  id: number
  channel_id: number | null
  role: SwitchgearBindingRole
  delay_ms: number
}

export interface SwitchgearBindingInput {
  channel_id: number | null
  role: SwitchgearBindingRole
  delay_ms?: number
}

export interface Switchgear {
  id: number
  project_id: number
  switchgear_type: SwitchgearType
  name: string
  bindings: SwitchgearBinding[]
}

export interface SwitchgearCreateInput {
  name: string
  switchgear_type?: SwitchgearType
  bindings?: SwitchgearBindingInput[]
}

export interface SwitchgearUpdateInput {
  name?: string
  switchgear_type?: SwitchgearType
  bindings?: SwitchgearBindingInput[]
}
