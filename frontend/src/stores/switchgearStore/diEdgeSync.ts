import type { Ref } from "vue"

import { CHANNEL_TYPES } from "@/types/channel"
import type { Switchgear } from "@/types/switchgear"
import { SWITCHGEAR_CODE, type SwitchgearState } from "@/constants/switchgear"

type LoggerLike = {
  debug: (message: string, ...args: unknown[]) => void
  warn: (message: string, ...args: unknown[]) => void
}

type DiEdgeMemory = Record<number, { diOpen: boolean | null; diClose: boolean | null }>

type DoPair = { unitId: string; chOpen: number; chClose: number }

type SendDoPairResult = { ok: boolean; error?: string | null }

type Params = {
  logger: LoggerLike
  switchgears: Ref<Switchgear[]>
  diEdgeMemory: Ref<DiEdgeMemory>
  bindingByRole: (sw: Switchgear, role: "di_open" | "di_close" | "do_open" | "do_closed") => { delay_ms?: number | null } | null
  resolveBindingChannelId: (sw: Switchgear, roles: Array<"di_open" | "di_close" | "do_open" | "do_closed">) => number | null
  resolveBinaryState: (channelId: number | null, expectedType: typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES]) => boolean | null
  resolveTypedChannel: (channelId: number | null, expectedType: typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES]) => { device_id: number; index: number; state?: unknown } | null
  resolveSwitchgearState: (sw: Switchgear) => SwitchgearState
  resolveDoPair: (sw: Switchgear) => DoPair | null
  didChannelChangeInLastRevision: (deviceId: number, chIndex: number) => boolean
  hasPendingCommandForUnit: (unitId: string) => boolean
  sendDoPairCommand: (
    unitId: string,
    chA: number,
    chB: number,
    state2b: 0 | 1 | 2 | 3,
    options?: { source?: string },
  ) => SendDoPairResult
}

export function createSwitchgearDiEdgeSync(params: Params) {
  const diDelayTimers = new Map<number, ReturnType<typeof setTimeout>>()

  function clearDiDelayTimer(switchgearId: number) {
    const timer = diDelayTimers.get(switchgearId)
    if (!timer) return
    clearTimeout(timer)
    diDelayTimers.delete(switchgearId)
  }

  function clearAllDiDelayTimers() {
    for (const timer of diDelayTimers.values()) {
      clearTimeout(timer)
    }
    diDelayTimers.clear()
  }

  function syncDiDrivenSwitching() {
    const activeIds = new Set<number>()

    for (const sw of params.switchgears.value) {
      activeIds.add(sw.id)
      const diOpenChannel = params.resolveTypedChannel(
        params.resolveBindingChannelId(sw, ["di_open"]),
        CHANNEL_TYPES.DI,
      )
      const diCloseChannel = params.resolveTypedChannel(
        params.resolveBindingChannelId(sw, ["di_close"]),
        CHANNEL_TYPES.DI,
      )
      const diOpen = diOpenChannel && typeof diOpenChannel.state === "boolean"
        ? diOpenChannel.state
        : null
      const diClose = diCloseChannel && typeof diCloseChannel.state === "boolean"
        ? diCloseChannel.state
        : null

      const diChanged = Boolean(
        (diOpenChannel && params.didChannelChangeInLastRevision(diOpenChannel.device_id, diOpenChannel.index)) ||
        (diCloseChannel && params.didChannelChangeInLastRevision(diCloseChannel.device_id, diCloseChannel.index)),
      )
      if (!diChanged) {
        continue
      }

      const prev = params.diEdgeMemory.value[sw.id] ?? { diOpen: null, diClose: null }
      const openEdge = prev.diOpen === false && diOpen === true && diClose === false
      const closeEdge = prev.diClose === false && diClose === true && diOpen === false

      if (openEdge !== closeEdge) {
        const targetStateName: SwitchgearState = openEdge ? "OPEN" : "CLOSED"
        const feedbackDelayMsRaw = params.bindingByRole(sw, openEdge ? "di_open" : "di_close")?.delay_ms
        const feedbackDelayMs = Number.isFinite(feedbackDelayMsRaw as number)
          ? Math.max(0, Math.round(Number(feedbackDelayMsRaw)))
          : 0

        clearDiDelayTimer(sw.id)

        const runDiDrivenSwitch = () => {
          const latest = params.switchgears.value.find(item => item.id === sw.id)
          if (!latest) return

          const latestDiOpen = params.resolveBinaryState(
            params.resolveBindingChannelId(latest, ["di_open"]),
            CHANNEL_TYPES.DI,
          )
          const latestDiClose = params.resolveBinaryState(
            params.resolveBindingChannelId(latest, ["di_close"]),
            CHANNEL_TYPES.DI,
          )

          const targetStillActive = targetStateName === "OPEN"
            ? latestDiOpen === true && latestDiClose === false
            : latestDiClose === true && latestDiOpen === false

          if (!targetStillActive) {
            params.logger.debug(
              `Skip delayed DI-driven ${targetStateName} for switchgear ${sw.id}: DI state changed during feedback delay`,
            )
            return
          }

          const currentState = params.resolveSwitchgearState(latest)
          const doPair = params.resolveDoPair(latest)
          if (!doPair || currentState === targetStateName) {
            return
          }

          if (params.hasPendingCommandForUnit(doPair.unitId)) {
            params.logger.debug(
              `Skip DI-driven ${targetStateName} for switchgear ${sw.id}: unit ${doPair.unitId} has pending command`,
            )
            return
          }

          const targetState = targetStateName === "OPEN" ? SWITCHGEAR_CODE.OPEN : SWITCHGEAR_CODE.CLOSED
          const result = params.sendDoPairCommand(
            doPair.unitId,
            doPair.chOpen,
            doPair.chClose,
            targetState,
            { source: "switchgear-di-edge" },
          )

          if (!result.ok) {
            params.logger.warn(
              `DI edge failed for switchgear ${sw.id} -> ${targetStateName}: ${result.error ?? "unknown"}`,
            )
          } else {
            params.logger.debug(
              `DI edge drove switchgear ${sw.id} -> ${targetStateName} via ${doPair.unitId} [${doPair.chOpen}/${doPair.chClose}]`,
            )
          }
        }

        if (feedbackDelayMs > 0) {
          params.logger.debug(
            `Schedule DI-driven ${targetStateName} for switchgear ${sw.id} in ${feedbackDelayMs}ms`,
          )
          const timer = setTimeout(() => {
            diDelayTimers.delete(sw.id)
            runDiDrivenSwitch()
          }, feedbackDelayMs)
          diDelayTimers.set(sw.id, timer)
        } else {
          runDiDrivenSwitch()
        }
      }

      params.diEdgeMemory.value[sw.id] = { diOpen, diClose }
    }

    for (const key of Object.keys(params.diEdgeMemory.value)) {
      const switchgearId = Number(key)
      if (!activeIds.has(switchgearId)) {
        delete params.diEdgeMemory.value[switchgearId]
        clearDiDelayTimer(switchgearId)
      }
    }
  }

  return {
    clearDiDelayTimer,
    clearAllDiDelayTimers,
    syncDiDrivenSwitching,
  }
}
