import { CHANNEL_TYPES, type Channel, type DoChannel, type DoChannelUiState, type TimeoutHandle } from "@/types/channel"

const COMMAND_PENDING_DEBOUNCE_MS = 50
const COMMAND_TIMEOUT_MS = 2000
const COMMAND_FAILURE_DISPLAY_MS = 2000
const COMMAND_STATE_REFRESH_FALLBACK_MS = 120

type RequestStatesFn = (deviceId: number, options?: { includeDiagnostics?: boolean; silent?: boolean }) => void

type CreateChannelCommandRuntimeParams = {
  channelsByDeviceFast: (deviceId: number) => readonly Channel[]
  requestStates: RequestStatesFn
}

export function createChannelCommandRuntime(params: CreateChannelCommandRuntimeParams) {
  const actionCounters = new Map<number, number>()
  const actionQueues = new Map<number, string[]>()
  const aoActionMap = new Map<string, string>()
  const doStateRefreshTimers = new Map<number, TimeoutHandle>()

  function enqueueAction(deviceId: number): string {
    const next = (actionCounters.get(deviceId) ?? 0) + 1
    actionCounters.set(deviceId, next)
    const actionId = next.toString().padStart(4, "0")
    const queue = actionQueues.get(deviceId) ?? []
    queue.push(actionId)
    actionQueues.set(deviceId, queue)
    return actionId
  }

  function removeAction(deviceId: number, actionId?: string) {
    if (!actionId) return
    const queue = actionQueues.get(deviceId)
    if (!queue) return
    const idx = queue.indexOf(actionId)
    if (idx === -1) return
    queue.splice(idx, 1)
    if (!queue.length) {
      actionQueues.delete(deviceId)
    }
  }

  function aoKey(deviceId: number, chIndex: number) {
    return `${deviceId}:${chIndex}`
  }

  function registerAoAction(deviceId: number, chIndex: number, actionId: string) {
    aoActionMap.set(aoKey(deviceId, chIndex), actionId)
  }

  function peekAoAction(deviceId: number, chIndex: number): string | undefined {
    return aoActionMap.get(aoKey(deviceId, chIndex))
  }

  function clearAoAction(deviceId: number, chIndex: number) {
    aoActionMap.delete(aoKey(deviceId, chIndex))
  }

  function ensureDoUi(channel: DoChannel): DoChannelUiState {
    if (!channel.ui) {
      channel.ui = { stage: "idle" }
    }
    return channel.ui
  }

  function clearDoUiTimers(ui: DoChannelUiState) {
    if (ui.debounceTimer) {
      clearTimeout(ui.debounceTimer)
      ui.debounceTimer = null
    }
    if (ui.timeoutTimer) {
      clearTimeout(ui.timeoutTimer)
      ui.timeoutTimer = null
    }
    if (ui.errorTimer) {
      clearTimeout(ui.errorTimer)
      ui.errorTimer = null
    }
  }

  function resetDoUiState(channel: DoChannel) {
    const ui = ensureDoUi(channel)
    clearDoUiTimers(ui)
    ui.stage = "idle"
    ui.target = undefined
    ui.previous = undefined
    ui.actionId = undefined
  }

  function enterDoPendingState(channel: DoChannel, target: boolean, actionId?: string) {
    const ui = ensureDoUi(channel)
    clearDoUiTimers(ui)
    ui.stage = "debounce"
    ui.target = target
    ui.previous = channel.state
    ui.actionId = actionId

    ui.debounceTimer = setTimeout(() => {
      ui.stage = "pending"
    }, COMMAND_PENDING_DEBOUNCE_MS)

    ui.timeoutTimer = setTimeout(() => {
      ui.stage = "error"
      ui.target = undefined
      if (ui.actionId) {
        removeAction(channel.device_id, ui.actionId)
      }
      if (typeof ui.previous === "boolean") {
        channel.state = ui.previous
      }
      ui.errorTimer = setTimeout(() => {
        resetDoUiState(channel)
      }, COMMAND_FAILURE_DISPLAY_MS)
    }, COMMAND_TIMEOUT_MS)
  }

  function fulfillDoPendingState(channel: DoChannel, actualState: boolean) {
    const ui = channel.ui
    if (!ui) {
      return
    }
    const actionId = ui.actionId

    if (ui.target === actualState || ui.stage === "error") {
      if (actionId) {
        removeAction(channel.device_id, actionId)
      }
      resetDoUiState(channel)
      return
    }
    // Keep pending/debounce active if the observed state does not match the target yet.
    // This is important for pair commands where partial/intermediate updates can arrive first.
  }

  function hasPendingDoForDevice(deviceId: number): boolean {
    for (const channel of params.channelsByDeviceFast(deviceId)) {
      if (channel.type !== CHANNEL_TYPES.DO) {
        continue
      }
      const stage = (channel as DoChannel).ui?.stage
      if (stage === "pending" || stage === "debounce") {
        return true
      }
    }
    return false
  }

  function scheduleDoStateRefreshIfPending(deviceId: number, commandIssuedAt: number) {
    const existing = doStateRefreshTimers.get(deviceId)
    if (existing) {
      clearTimeout(existing)
    }
    const timer = setTimeout(() => {
      doStateRefreshTimers.delete(deviceId)
      if (!hasPendingDoForDevice(deviceId)) {
        return
      }
      // Any state event (including diagnostics / unrelated channel updates) may arrive
      // after the command and should not suppress the fallback DO state request.
      // The authoritative guard is the actual pending UI state above.
      params.requestStates(deviceId, { includeDiagnostics: false, silent: true })
    }, COMMAND_STATE_REFRESH_FALLBACK_MS)
    doStateRefreshTimers.set(deviceId, timer)
  }

  function clearDoStateRefreshTimers() {
    for (const timer of doStateRefreshTimers.values()) {
      clearTimeout(timer)
    }
    doStateRefreshTimers.clear()
  }

  return {
    enqueueAction,
    removeAction,
    registerAoAction,
    peekAoAction,
    clearAoAction,
    clearDoUiTimers,
    resetDoUiState,
    enterDoPendingState,
    fulfillDoPendingState,
    hasPendingDoForDevice,
    scheduleDoStateRefreshIfPending,
    clearDoStateRefreshTimers,
  }
}
