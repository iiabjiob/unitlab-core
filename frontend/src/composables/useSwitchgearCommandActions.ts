import { computed, onBeforeUnmount, ref, watch, type Ref } from "vue"
import type { Switchgear } from "@/types/switchgear"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useToastStore } from "@/stores/toastStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import { useSwitchgearLogStore } from "@/stores/switchgearLogStore"
import { SWITCHGEAR_CODE, type SwitchgearState } from "@/constants/switchgear"

export type SwitchgearCommandAction = "open" | "close" | "intermediate" | "unknown"

export function useSwitchgearCommandActions(
  switchgear: Ref<Switchgear>,
  options: { source?: string } = {},
) {
  const switchgearStore = useSwitchgearStore()
  const channelStore = useChannelStore()
  const deviceStore = useDeviceStore()
  const toastStore = useToastStore()
  const wsStore = useWebSocketStore()
  const switchgearLogStore = useSwitchgearLogStore()

  const acting = ref<SwitchgearCommandAction | null>(null)
  const pendingPairTargetState = ref<SwitchgearState | null>(null)
  const expectedCommandResponses = ref(0)
  let commandWatchdog: ReturnType<typeof setTimeout> | null = null

  function clearCommandWatchdog() {
    if (!commandWatchdog) return
    clearTimeout(commandWatchdog)
    commandWatchdog = null
  }

  function channelForRole(role: "do_open" | "do_closed") {
    const binding = switchgearStore.bindingByRole(switchgear.value, role)
    if (!binding?.channel_id) return null
    return channelStore.channels.find(ch => ch.id === binding.channel_id) ?? null
  }

  function isChannelOnline(channelId: number | null): boolean {
    if (!Number.isFinite(channelId as number)) return false
    const channel = channelStore.channels.find(ch => ch.id === Number(channelId))
    if (!channel) return false
    const device = deviceStore.devices.find(item => item.id === channel.device_id)
    return device?.status === "online"
  }

  const doOpenChannel = computed(() => channelForRole("do_open"))
  const doCloseChannel = computed(() => channelForRole("do_closed"))

  const doOpenOnline = computed(() => isChannelOnline(doOpenChannel.value?.id ?? null))
  const doCloseOnline = computed(() => isChannelOnline(doCloseChannel.value?.id ?? null))
  const pairUnitId = computed(() => (
    doOpenChannel.value ? channelStore.resolveUnitId(doOpenChannel.value.device_id) : null
  ))
  const pairPending = computed(() => (
    pairUnitId.value ? channelStore.hasPendingCommandForUnit(pairUnitId.value) : false
  ))
  const pairResponse = computed(() => (
    pairUnitId.value ? (channelStore.responses[pairUnitId.value] ?? null) : null
  ))

  const hasPairTargets = computed(() => !!doOpenChannel.value && !!doCloseChannel.value)
  const pairSameUnit = computed(() => {
    if (!doOpenChannel.value || !doCloseChannel.value) return false
    return doOpenChannel.value.device_id === doCloseChannel.value.device_id
  })
  const pairOnline = computed(() => doOpenOnline.value && doCloseOnline.value)
  const canPairCommand = computed(() => (
    hasPairTargets.value &&
    pairSameUnit.value &&
    pairOnline.value &&
    acting.value === null &&
    !pairPending.value
  ))

  const positionState = computed(() => switchgearStore.resolveSwitchgearState(switchgear.value))
  const positionStateLabel = computed(() => (
    positionState.value === "INTERMEDIATE" ? "UNDEFINED" : positionState.value
  ))
  const canOpen = computed(() => canPairCommand.value && positionState.value !== "OPEN")
  const canClose = computed(() => canPairCommand.value && positionState.value !== "CLOSED")
  const canUndefined = computed(() => canPairCommand.value && positionState.value !== "INTERMEDIATE")
  const canUnknown = computed(() => canPairCommand.value && positionState.value !== "UNKNOWN")
  const positionVariant = computed(() => {
    switch (positionState.value) {
      case "OPEN":
        return "success"
      case "CLOSED":
        return "danger"
      case "INTERMEDIATE":
        return "warning"
      default:
        return "neutral"
    }
  })

  const unitOnline = computed(() => switchgearStore.isUnitOnline(switchgear.value))

  watch(pairPending, (pending) => {
    if (!pending) {
      acting.value = null
      pendingPairTargetState.value = null
      expectedCommandResponses.value = 0
      clearCommandWatchdog()
    }
  })

  watch(
    () => pairResponse.value ? `${pairResponse.value.packet_id}:${pairResponse.value.status}:${pairResponse.value.error ?? ""}` : null,
    (key) => {
      if (!key || !pairResponse.value) return
      if (expectedCommandResponses.value <= 0) return
      expectedCommandResponses.value -= 1
      if (pairResponse.value.status === "OK") {
        switchgearLogStore.push(switchgear.value.id, {
          type: "info",
          message: `ACK packet #${pairResponse.value.packet_id} from ${pairResponse.value.unit_id}`,
        })
        return
      }
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `Command error #${pairResponse.value.packet_id}: ${pairResponse.value.status}${pairResponse.value.error ? ` (${pairResponse.value.error})` : ""}`,
      })
      pendingPairTargetState.value = null
      acting.value = null
    },
  )

  watch(
    () => positionState.value,
    (next, prev) => {
      if (!prev || next === prev) return
      if (pendingPairTargetState.value && next !== pendingPairTargetState.value) {
        return
      }
      const label = next === "INTERMEDIATE" ? "UNDEFINED" : next
      switchgearLogStore.push(switchgear.value.id, {
        type: "info",
        message: `Position changed -> ${label}`,
      })
    },
  )

  function stateCodeForAction(action: SwitchgearCommandAction): 0 | 1 | 2 | 3 {
    switch (action) {
      case "open":
        return SWITCHGEAR_CODE.OPEN
      case "close":
        return SWITCHGEAR_CODE.CLOSED
      case "intermediate":
        return SWITCHGEAR_CODE.INTERMEDIATE
      case "unknown":
        return SWITCHGEAR_CODE.UNKNOWN
    }
  }

  function actionLabel(action: SwitchgearCommandAction): string {
    switch (action) {
      case "open":
        return "Open"
      case "close":
        return "Close"
      case "intermediate":
        return "Undefined"
      case "unknown":
        return "Unknown"
    }
  }

  function targetStateForAction(action: SwitchgearCommandAction): SwitchgearState {
    switch (action) {
      case "open":
        return "OPEN"
      case "close":
        return "CLOSED"
      case "intermediate":
        return "INTERMEDIATE"
      case "unknown":
        return "UNKNOWN"
    }
  }

  async function sendSwitchgearCommand(action: SwitchgearCommandAction) {
    expectedCommandResponses.value = 0
    if (!doOpenChannel.value || !doCloseChannel.value) {
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `${actionLabel(action)} rejected: both DO command channels must be bound`,
      })
      toastStore.warning("Both DO command channels must be bound")
      return
    }
    if (!pairSameUnit.value) {
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `${actionLabel(action)} rejected: DO channels are on different units`,
      })
      toastStore.warning("Pair command requires both DO channels on the same unit")
      return
    }
    if (!pairOnline.value) {
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `${actionLabel(action)} rejected: target unit is offline`,
      })
      toastStore.warning("Cannot send pair command: unit is offline")
      return
    }
    if (doOpenChannel.value.type !== "do" || doCloseChannel.value.type !== "do") {
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `${actionLabel(action)} rejected: pair command requires DO channels`,
      })
      toastStore.error("Pair command requires DO channels")
      return
    }
    if (!wsStore.isConnected) {
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `${actionLabel(action)} rejected: WebSocket is disconnected`,
      })
      toastStore.error("WebSocket disconnected. Command not sent.")
      return
    }
    if (acting.value || pairPending.value) {
      return
    }

    const code = stateCodeForAction(action)
    const unitId = pairUnitId.value
    if (!unitId) {
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: `${actionLabel(action)} rejected: cannot resolve unit id`,
      })
      toastStore.error("Cannot resolve unit id for pair command")
      return
    }

    acting.value = action
    pendingPairTargetState.value = targetStateForAction(action)

    try {
      const result = channelStore.sendDoPairCommand(
        unitId,
        doOpenChannel.value.index,
        doCloseChannel.value.index,
        code,
        { source: options.source?.trim() || "switchgear-ui" },
      )
      if (!result.ok) {
        switchgearLogStore.push(switchgear.value.id, {
          type: "error",
          message: result.error,
        })
        toastStore.error(result.error)
        acting.value = null
        return
      }

      switchgearLogStore.push(switchgear.value.id, {
        type: "command",
        message: `${actionLabel(action)} command queued on ${unitId} [CH${doOpenChannel.value.index + 1}/CH${doCloseChannel.value.index + 1}]`,
      })
      expectedCommandResponses.value = 1
      clearCommandWatchdog()
      commandWatchdog = setTimeout(() => {
        if (!acting.value) return
        switchgearLogStore.push(switchgear.value.id, {
          type: "error",
          message: `${actionLabel(action)} command timed out waiting for state feedback`,
        })
        expectedCommandResponses.value = 0
        pendingPairTargetState.value = null
        acting.value = null
        commandWatchdog = null
      }, 3000)
      toastStore.success(`${actionLabel(action)} command queued`)
    } catch (error) {
      expectedCommandResponses.value = 0
      pendingPairTargetState.value = null
      toastStore.error(error instanceof Error ? error.message : "Failed to send pair command")
      switchgearLogStore.push(switchgear.value.id, {
        type: "error",
        message: error instanceof Error ? error.message : "Failed to send pair command",
      })
      acting.value = null
    } finally {
      if (!pairPending.value) {
        pendingPairTargetState.value = null
        acting.value = null
        clearCommandWatchdog()
      }
    }
  }

  onBeforeUnmount(() => {
    clearCommandWatchdog()
  })

  return {
    acting,
    canClose,
    canOpen,
    canPairCommand,
    canUndefined,
    canUnknown,
    doCloseChannel,
    doOpenChannel,
    hasPairTargets,
    pairOnline,
    pairPending,
    pairSameUnit,
    positionState,
    positionStateLabel,
    positionVariant,
    sendSwitchgearCommand,
    unitOnline,
  }
}
