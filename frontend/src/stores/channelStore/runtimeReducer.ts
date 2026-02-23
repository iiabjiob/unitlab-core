import type { Ref } from "vue"

import { formatAoValue } from "@/utils/channel"
import {
  applyAoDiagnostics,
  applyDeltaState,
  applyDiDiagnostics,
  applyDoDiagnostics,
  type AoDiagnosticsBitmasks,
  type DiDiagnosticsBitmasks,
  type DoDiagnosticsBitmasks,
} from "@/stores/utils/diagnostics"
import {
  StateMode,
  type DeviceRespEvent,
  type DeviceStateEvent,
} from "@/types/ws/events"
import { CHANNEL_TYPES, type AoChannel, type Channel, type DiChannel, type DoChannel } from "@/types/channel"

type DeviceLike = {
  id: number
  unit_id: string
  device_type: string
}

type LoggerLike = {
  debug: (message: string, ...args: unknown[]) => void
  warn: (message: string, ...args: unknown[]) => void
}

type ChannelLogPayload = {
  type: "cmd" | "state" | "error"
  message: string
  reason?: string
}

type Params = {
  logger: LoggerLike
  channels: Ref<Channel[]>
  responses: Ref<Record<string, DeviceRespEvent>>
  stateRevision: Ref<number>
  lastStateChangedKeys: Ref<Set<string>>
  deviceLastStateAt: Map<number, number>
  channelKey: (deviceId: number, chIndex: number) => string
  channelByDeviceAndIndex: (deviceId: number, chIndex: number) => Channel | undefined
  channelsByDeviceFast: (deviceId: number) => readonly Channel[]
  channelsByDevice: (deviceId: number) => Channel[]
  findDeviceByUnitId: (unitId: string) => DeviceLike | undefined
  fulfillDoPendingState: (channel: DoChannel, actualState: boolean) => void
  peekAoAction: (deviceId: number, chIndex: number) => string | undefined
  clearAoAction: (deviceId: number, chIndex: number) => void
  removeAction: (deviceId: number, actionId?: string) => void
  pushDeviceLog: (deviceId: number, entry: ChannelLogPayload, actionId?: string) => void
  toDigitalLabel: (value: unknown) => "ON" | "OFF"
  summarizeDoChannels: (deviceId: number) => { on: number; off: number; total: number }
  formatSummary: (summary: { on: number; off: number; total: number }) => string
  summarizeDoDiagnostics: (deviceId: number, diag: DoDiagnosticsBitmasks) => string
  logDiDiagnosticChanges: (deviceId: number, changes: ReturnType<typeof applyDiDiagnostics>) => void
  resolveChannelLabel: (ch: Channel) => string
}

export function createChannelRuntimeReducer(params: Params) {
  function applyBitState(deviceId: number, chIndex: number, value: boolean): { changed: boolean; actionId?: string } {
    const ch = params.channelByDeviceAndIndex(deviceId, chIndex)
    if (!ch) {
      return { changed: false }
    }
    if (ch.type === CHANNEL_TYPES.AO) {
      return { changed: false }
    }
    let actionId: string | undefined
    if (ch.type === CHANNEL_TYPES.DO) {
      actionId = ch.ui?.actionId
    }
    const previous = ch.state
    const changed = previous !== value
    if (changed) {
      ch.state = value
    }
    if (ch.type === CHANNEL_TYPES.DO) {
      params.fulfillDoPendingState(ch as DoChannel, value)
    }
    return { changed, actionId }
  }

  function applyBitmaskState(
    deviceId: number,
    mask: number,
  ): { changed: boolean; actionIds: Set<string>; changedIndexes: number[] } {
    let changed = false
    const actionIds = new Set<string>()
    const changedIndexes: number[] = []
    for (const ch of params.channelsByDeviceFast(deviceId)) {
      if (ch.type === CHANNEL_TYPES.AO) continue
      const next = ((mask >> ch.index) & 1) === 1
      if (ch.state !== next) {
        ch.state = next
        changed = true
        changedIndexes.push(ch.index)
      }
      if (ch.type === CHANNEL_TYPES.DO) {
        const id = ch.ui?.actionId
        if (id) {
          actionIds.add(id)
        }
        params.fulfillDoPendingState(ch as DoChannel, next)
      }
    }
    return { changed, actionIds, changedIndexes }
  }

  function countBits(mask: number): number {
    let value = mask >>> 0
    let count = 0
    while (value) {
      value &= value - 1
      count += 1
    }
    return count
  }

  function summarizeAoDiagnostics(diag: AoDiagnosticsBitmasks) {
    return {
      valid: countBits(diag.valid_mask),
      pending: countBits(diag.pending_mask),
      fault: countBits(diag.fault_mask),
      error: countBits(diag.error_mask),
    }
  }

  function applyFloatState(deviceId: number, chIndex: number, value: number): { changed: boolean; actionId?: string } {
    const ch = params.channelByDeviceAndIndex(deviceId, chIndex)
    if (!ch || ch.type !== CHANNEL_TYPES.AO) {
      return { changed: false }
    }
    const previous = ch.state
    const changed = previous !== value
    if (changed) {
      ch.state = value
    }
    const actionId = params.peekAoAction(deviceId, chIndex)
    return { changed, actionId }
  }

  function setChannels(event: DeviceStateEvent) {
    const device = params.findDeviceByUnitId(event.unit_id)
    if (!device) {
      params.logger.warn(`Device with unit_id=${event.unit_id} not found`)
      return
    }
    params.deviceLastStateAt.set(device.id, Date.now())
    let stateChanged = false
    const changedIndexes: number[] = []

    switch (event.mode) {
      case StateMode.STATE_SINGLE_BIT: {
        const { changed, actionId } = applyBitState(device.id, event.payload.ch, !!event.payload.value)
        if (!changed) {
          break
        }
        stateChanged = true
        changedIndexes.push(event.payload.ch)
        const singleLabel = params.toDigitalLabel(event.payload.value)
        params.pushDeviceLog(device.id, {
          type: "state",
          message: `CH${event.payload.ch + 1} state ${singleLabel}`,
        }, actionId)
        if (actionId) {
          params.removeAction(device.id, actionId)
        }
        break
      }
      case StateMode.STATE_ALL_BIT: {
        const { changed, actionIds, changedIndexes: indexes } = applyBitmaskState(device.id, event.payload.bitmask)
        if (!changed) {
          break
        }
        stateChanged = true
        changedIndexes.push(...indexes)
        const deviceType = device.device_type.toLowerCase()
        if (deviceType === "do") {
          const maskSummary = params.formatSummary(params.summarizeDoChannels(device.id))
          const actionId = actionIds.size === 1 ? Array.from(actionIds)[0] : undefined
          params.pushDeviceLog(device.id, {
            type: "state",
            message: `All digital outputs updated (${maskSummary})`,
          }, actionId)
          if (actionId) {
            params.removeAction(device.id, actionId)
          }
        }
        break
      }
      case StateMode.STATE_SINGLE_FLOAT: {
        const { changed, actionId } = applyFloatState(device.id, event.payload.ch, Number(event.payload.value))
        if (!changed) {
          break
        }
        stateChanged = true
        changedIndexes.push(event.payload.ch)
        const aoValue = formatAoValue(Number(event.payload.value))
        params.pushDeviceLog(device.id, {
          type: "state",
          message: `AO CH${event.payload.ch + 1} set to ${aoValue} mA`,
        }, actionId)
        if (actionId) {
          params.clearAoAction(device.id, event.payload.ch)
          params.removeAction(device.id, actionId)
        }
        break
      }
      case StateMode.DIAG_ALL_BIT: {
        const diagPayload: DoDiagnosticsBitmasks = {
          open_mask: Number(event.payload.open_mask) >>> 0,
          fault_mask: Number(event.payload.fault_mask) >>> 0,
          soft_mask: Number(event.payload.soft_mask) >>> 0,
        }
        const doChannels = params.channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.DO,
        ) as DoChannel[]
        const changed = applyDoDiagnostics(doChannels, diagPayload)
        if (!changed) {
          break
        }
        const summary = params.summarizeDoDiagnostics(device.id, diagPayload)
        params.pushDeviceLog(device.id, {
          type: "state",
          message: `Diagnostics updated (${summary})`,
        })
        break
      }
      case StateMode.DIAG_AO_FLOAT: {
        const diagPayload: AoDiagnosticsBitmasks = {
          valid_mask: Number(event.payload.valid_mask) >>> 0,
          pending_mask: Number(event.payload.pending_mask) >>> 0,
          fault_mask: Number(event.payload.fault_mask) >>> 0,
          error_mask: Number(event.payload.error_mask) >>> 0,
        }
        const aoChannels = params.channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.AO,
        ) as AoChannel[]
        const changes = applyAoDiagnostics(aoChannels, diagPayload)
        if (!changes.length) {
          break
        }
        const summary = summarizeAoDiagnostics(diagPayload)
        params.pushDeviceLog(device.id, {
          type: "state",
          message: `AO diagnostics updated (valid=${summary.valid}, pending=${summary.pending}, fault=${summary.fault}, error=${summary.error})`,
        })
        break
      }
      case StateMode.STATE_CHANGED_BIT: {
        const changedMask = Number(event.payload.changed) >>> 0
        const stateMask = Number(event.payload.state) >>> 0
        const { changed, actionIds, updates } = applyDeltaState(
          params.channels.value,
          device.id,
          changedMask,
          stateMask,
          { onDoUpdate: (channel, next) => params.fulfillDoPendingState(channel, next) },
        )
        if (!changed) {
          break
        }
        stateChanged = true
        for (const update of updates) {
          changedIndexes.push(update.channel.index)
        }
        const channelDescriptions = updates
          .map(({ channel, value }) => `${params.resolveChannelLabel(channel)} → ${params.toDigitalLabel(value)}`)
        const message = channelDescriptions.length
          ? `Digital delta: ${channelDescriptions.join(", ")}`
          : `Digital delta (${countBits(changedMask)} ch)`
        const actionId = actionIds.size === 1 ? Array.from(actionIds)[0] : undefined
        params.pushDeviceLog(device.id, {
          type: "state",
          message,
        }, actionId)
        if (actionId) {
          params.removeAction(device.id, actionId)
        }
        break
      }
      case StateMode.DIAG_DI_BIT: {
        const diagPayload: DiDiagnosticsBitmasks = {
          seen_mask: Number(event.payload.seen) >>> 0,
          stuck_mask: Number(event.payload.stuck) >>> 0,
          lost_mask: Number(event.payload.lost) >>> 0,
          latched_mask: Number(event.payload.latched) >>> 0,
          latched_changed_mask: Number(event.payload.latched_changed) >>> 0,
          latched_cause_mask: Number(event.payload.latched_cause) >>> 0,
        }
        const diChannels = params.channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.DI,
        ) as DiChannel[]
        const changes = applyDiDiagnostics(diChannels, diagPayload)
        if (!changes.length) {
          break
        }
        params.logDiDiagnosticChanges(device.id, changes)
        break
      }
      case StateMode.STATE_LATCHED_BIT: {
        const diagPayload: DiDiagnosticsBitmasks = {
          latched_mask: Number(event.payload.latched) >>> 0,
          latched_changed_mask: Number(event.payload.changed) >>> 0,
          latched_cause_mask: Number(event.payload.cause) >>> 0,
        }
        const diChannels = params.channelsByDevice(device.id).filter(
          ch => ch.type === CHANNEL_TYPES.DI,
        ) as DiChannel[]
        const changes = applyDiDiagnostics(diChannels, diagPayload)
        if (!changes.length) {
          break
        }
        params.logDiDiagnosticChanges(device.id, changes)
        break
      }
      default:
        params.logger.debug(`Unhandled device state mode=${event.mode}`)
    }

    if (stateChanged) {
      params.lastStateChangedKeys.value = new Set(
        changedIndexes.map(index => params.channelKey(device.id, index)),
      )
      params.stateRevision.value += 1
    }
  }

  function didChannelChangeInLastRevision(deviceId: number, chIndex: number): boolean {
    return params.lastStateChangedKeys.value.has(params.channelKey(deviceId, chIndex))
  }

  function setResponse(resp: DeviceRespEvent) {
    params.responses.value = {
      ...params.responses.value,
      [resp.unit_id]: resp,
    }

    const device = params.findDeviceByUnitId(resp.unit_id)
    if (!device) {
      params.logger.warn(`DeviceRespEvent: device not found for unit_id=${resp.unit_id}`)
      return
    }

    if (resp.status === "OK") {
      params.logger.debug(`✅ Device response from ${resp.unit_id}, packet=${resp.packet_id}, status=${resp.status}`)
    } else {
      params.logger.debug(
        `⚠️ Command resp from ${resp.unit_id}, packet=${resp.packet_id}, status=${resp.status}, error=${resp.error}`,
      )
      params.logger.warn(
        `⚠️ Command response from ${resp.unit_id}: status=${resp.status}${resp.error ? `, error=${resp.error}` : ""}`,
      )
      const errorDetail = resp.error ? `: ${resp.error}` : ""
      params.pushDeviceLog(device.id, {
        type: "error",
        message: `Command error (${resp.status})${errorDetail}`,
      })
    }
  }

  return {
    setChannels,
    setResponse,
    didChannelChangeInLastRevision,
  }
}
