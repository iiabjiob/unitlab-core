import { formatAoValue } from "@/utils/channel"
import { useDeviceStore } from "@/stores/deviceStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import {
  WSAction,
  CmdMode,
  type SetDoCommandMessage,
  type SetAoCommandMessage,
} from "@/types/ws/messages"
import { CHANNEL_TYPES, type Channel, type DoChannel } from "@/types/channel"

const AO_STATE_REFRESH_FALLBACK_MS = 100
const DO_BULK_STATE_RECONCILE_MS = 120

type LoggerLike = {
  info: (message: string) => void
  error: (message: string) => void
}

type DigitalStableLabel = "ON" | "OFF"
type RequestStatesFn = (deviceId: number, options?: { includeDiagnostics?: boolean; silent?: boolean }) => void

type ChannelLogPayload = {
  type: "cmd" | "state" | "error"
  message: string
  reason?: string
}

type SendDoPairResult = { ok: true; deviceId: number; actionId: string } | { ok: false; error: string }

type Params = {
  logger: LoggerLike
  channelsByDevice: (deviceId: number) => Channel[]
  findDoChannel: (deviceId: number, chIndex: number) => DoChannel | undefined
  enqueueAction: (deviceId: number) => string
  enterDoPendingState: (channel: DoChannel, target: boolean, actionId?: string) => void
  resetDoUiState: (channel: DoChannel) => void
  scheduleDoStateRefreshIfPending: (deviceId: number, commandIssuedAt: number) => void
  requestStates: RequestStatesFn
  registerAoAction: (deviceId: number, chIndex: number, actionId: string) => void
  pushDeviceLog: (deviceId: number, entry: ChannelLogPayload, actionId?: string) => void
  toDigitalLabel: (value: unknown) => DigitalStableLabel
  formatPendingSuffix: (active?: boolean) => string
  summarizeMaskTargets: (doChannels: DoChannel[], mask: number) => { on: number; off: number; total: number }
  formatSummary: (summary: { on: number; off: number; total: number }) => string
  decodePairTargets: (state2b: number) => [boolean, boolean] | null
  decodePairLabels: (state2b: number) => [DigitalStableLabel, DigitalStableLabel] | null
}

export function createChannelTransportActions(params: Params) {
  function applyOptimisticDoState(channel: DoChannel, target: boolean) {
    channel.state = target
  }

  function sendDoCommand(unitId: string, chIndex: number, state: boolean) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      params.logger.error(`Device ${unitId} not found for DO command`)
      return
    }

    const ws = useWebSocketStore()
    const actionId = params.enqueueAction(device.id)
    const msg: SetDoCommandMessage = {
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_SINGLE_BIT,
      ch: chIndex,
      value: state ? 1 : 0,
    }
    const commandIssuedAt = Date.now()
    ws.send(msg)

    const channel = params.findDoChannel(device.id, chIndex)
    if (channel) {
      params.enterDoPendingState(channel, state, actionId)
    }

    params.scheduleDoStateRefreshIfPending(device.id, commandIssuedAt)
    const targetLabel = params.toDigitalLabel(state)
    params.logger.info(`➡️ DO cmd ${device.unit_id} ch=${chIndex} → ${targetLabel}`)

    params.pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO CH${chIndex + 1} → ${targetLabel}${params.formatPendingSuffix(true)}`,
    }, actionId)
  }

  function sendDoAllCommand(deviceId: number, unitId: string, mask: number) {
    const deviceStore = useDeviceStore()
    const device =
      deviceStore.devices.find(d => d.id === deviceId) ||
      deviceStore.devices.find(d => d.unit_id === unitId)

    if (!device) {
      params.logger.error(`Device ${unitId} not found for DO all command`)
      return
    }

    const doChannels = params.channelsByDevice(device.id).filter(
      ch => ch.type === CHANNEL_TYPES.DO,
    ) as DoChannel[]

    const ws = useWebSocketStore()
    const maskSummary = params.formatSummary(params.summarizeMaskTargets(doChannels, mask))
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: unitId,
      mode: CmdMode.SET_ALL_BIT,
      bitmask: mask,
    } satisfies SetDoCommandMessage)

    doChannels.forEach(ch => {
      const target = ((mask >>> ch.index) & 1) === 1
      params.resetDoUiState(ch)
      applyOptimisticDoState(ch, target)
    })

    setTimeout(() => {
      params.requestStates(device.id, { includeDiagnostics: false, silent: true })
    }, DO_BULK_STATE_RECONCILE_MS)

    params.logger.info(`➡️ DO ALL cmd ${unitId} targets → ${maskSummary}`)

    params.pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO ALL (${maskSummary})`,
    })
  }

  function sendDoPairCommand(
    unitId: string,
    chA: number,
    chB: number,
    state2b: 0 | 1 | 2 | 3,
    options: { source?: string } = {},
  ): SendDoPairResult {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      const error = `Device ${unitId} not found for DO pair command`
      params.logger.error(error)
      return { ok: false, error }
    }
    if (chA === chB) {
      const error = `Invalid DO pair command for ${unitId}: channels must differ (ch=${chA})`
      params.logger.error(error)
      return { ok: false, error }
    }

    const pairTargets = params.decodePairTargets(state2b)
    if (!pairTargets) {
      const error = `Invalid DO pair state code for ${unitId}: ${state2b}`
      params.logger.error(error)
      return { ok: false, error }
    }

    const channelA = params.findDoChannel(device.id, chA)
    const channelB = params.findDoChannel(device.id, chB)
    if (!channelA || !channelB) {
      const error = `DO pair channels not found on ${unitId}: [${chA}/${chB}]`
      params.logger.error(error)
      return { ok: false, error }
    }

    const ws = useWebSocketStore()
    const actionId = params.enqueueAction(device.id)
    const commandIssuedAt = Date.now()
    ws.send({
      action: WSAction.SET_DO_COMMAND,
      unit_id: device.unit_id,
      mode: CmdMode.SET_PAIR_BIT,
      chA,
      chB,
      state2b,
    } satisfies SetDoCommandMessage)

    params.enterDoPendingState(channelA, pairTargets[0], actionId)
    params.enterDoPendingState(channelB, pairTargets[1], actionId)
    applyOptimisticDoState(channelA, pairTargets[0])
    applyOptimisticDoState(channelB, pairTargets[1])

    params.scheduleDoStateRefreshIfPending(device.id, commandIssuedAt)

    const pairLabels = params.decodePairLabels(state2b)
    const labelA = pairLabels?.[0] ?? "N/A"
    const labelB = pairLabels?.[1] ?? "N/A"
    const source = options.source?.trim() || "unknown"
    params.logger.info(
      `➡️ DO pair cmd ${device.unit_id} [${chA}/${chB}] → CH${chA + 1}:${labelA} / CH${chB + 1}:${labelB} (src=${source})`,
    )
    params.pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested DO pair CH${chA + 1} → ${labelA}, CH${chB + 1} → ${labelB}${params.formatPendingSuffix(Boolean(pairLabels))} [${source}]`,
    }, actionId)
    return { ok: true, deviceId: device.id, actionId }
  }

  function sendAoCommand(unitId: string, chIndex: number, value: number) {
    const deviceStore = useDeviceStore()
    const device = deviceStore.devices.find(d => d.unit_id === unitId)
    if (!device) {
      params.logger.error(`Device ${unitId} not found for AO command`)
      return
    }

    const ws = useWebSocketStore()
    const actionId = params.enqueueAction(device.id)
    ws.send({
      action: WSAction.SET_AO_COMMAND,
      unit_id: device.unit_id,
      ch: chIndex,
      value,
    } satisfies SetAoCommandMessage)

    params.registerAoAction(device.id, chIndex, actionId)

    // AO value and diagnostics are separate frames; pull both after the command to avoid
    // showing a stale number without the corresponding quality/fault state.
    setTimeout(() => {
      params.requestStates(device.id, { includeDiagnostics: true, silent: true })
    }, AO_STATE_REFRESH_FALLBACK_MS)
    const aoValue = formatAoValue(value)
    params.logger.info(`➡️ AO cmd ${device.unit_id} ch=${chIndex} → ${aoValue} mA`)

    params.pushDeviceLog(device.id, {
      type: "cmd",
      message: `User requested AO CH${chIndex + 1} → ${aoValue} mA`,
    }, actionId)
  }

  return {
    sendDoCommand,
    sendDoAllCommand,
    sendDoPairCommand,
    sendAoCommand,
  }
}
