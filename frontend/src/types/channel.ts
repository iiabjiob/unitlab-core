export const CHANNEL_TYPES = {
  DI: "di" as const,
  DO: "do" as const,
  AO: "ao" as const,
}

export type ChannelType = typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES]

export type ChannelUiStage = "idle" | "debounce" | "pending" | "error"

export type TimeoutHandle = ReturnType<typeof setTimeout>

export interface DoChannelUiState {
  stage: ChannelUiStage
  target?: boolean
  previous?: boolean
  actionId?: string
  debounceTimer?: TimeoutHandle | null
  timeoutTimer?: TimeoutHandle | null
  errorTimer?: TimeoutHandle | null
}

export interface ChannelDto {
  id: number
  device_id: number
  channel_index: number
  channel_type: ChannelType
  name?: string | null
  resolved_name?: string | null
  state?: boolean | number | null
  created_at?: string
  updated_at?: string
}

export interface ChannelBase {
  id: number
  device_id: number
  index: number
  type: ChannelType
  name: string
  resolved_name: string
  created_at?: number
  updated_at?: number
}

export interface ChannelDiagnostics {
  open: boolean
  fault: boolean
  soft: boolean
}

export interface DiChannelDiagnostics {
  seen: boolean
  stuck: boolean
  lost: boolean
  latched: boolean
  latchedChanged: boolean
  latchedCause: boolean
}

export interface DiChannel extends ChannelBase {
  type: "di"
  state: boolean
  diDiagnostics?: DiChannelDiagnostics
}

export interface DoChannel extends ChannelBase {
  type: "do"
  state: boolean
  ui?: DoChannelUiState
  diagnostics?: ChannelDiagnostics
}

export interface AoChannel extends ChannelBase {
  type: "ao"
  state: number
}

export type Channel = DiChannel | DoChannel | AoChannel

