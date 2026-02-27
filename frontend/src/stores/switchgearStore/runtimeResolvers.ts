import type { Ref } from "vue"

import { CHANNEL_TYPES, type Channel } from "@/types/channel"
import type { Switchgear, SwitchgearBindingRole } from "@/types/switchgear"
import { codeToState, type SwitchgearState } from "@/constants/switchgear"

type DeviceLike = {
  id: number
  status?: string | null
}

type Params = {
  switchgears: Ref<Switchgear[]>
  channels: Ref<Channel[]>
  devices: Ref<DeviceLike[]>
  resolveUnitId: (deviceId: number) => string
}

export function createSwitchgearRuntimeResolvers(params: Params) {
  function bindingByRole(sw: Switchgear, role: SwitchgearBindingRole) {
    return sw.bindings.find(binding => binding.role === role) ?? null
  }

  function resolveBindingChannelId(sw: Switchgear, roles: SwitchgearBindingRole[]) {
    for (const role of roles) {
      const candidate = bindingByRole(sw, role)?.channel_id ?? null
      if (candidate !== null && candidate !== undefined) {
        return candidate
      }
    }
    return null
  }

  function isChannelOnline(channelId: number | null): boolean {
    if (!channelId) return false
    const channel = params.channels.value.find(item => item.id === channelId)
    if (!channel) return false
    const device = params.devices.value.find(item => item.id === channel.device_id)
    return device?.status === "online"
  }

  function resolveBinaryState(
    channelId: number | null,
    expectedType: typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES],
  ): boolean | null {
    if (!channelId) return null
    const ch = params.channels.value.find(c => c.id === channelId)
    if (!ch || ch.type !== expectedType) return null
    return typeof ch.state === "boolean" ? ch.state : null
  }

  function resolveTypedChannel(
    channelId: number | null,
    expectedType: typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES],
  ): Channel | null {
    if (!channelId) return null
    const channel = params.channels.value.find(item => item.id === channelId) ?? null
    if (!channel || channel.type !== expectedType) {
      return null
    }
    return channel
  }

  function resolvePairState(
    open: boolean | null,
    closed: boolean | null,
  ): SwitchgearState | null {
    if (open === null || closed === null) return null
    return codeToState(open, closed)
  }

  function resolveDoPair(sw: Switchgear): { unitId: string; chOpen: number; chClose: number } | null {
    const doOpenId = resolveBindingChannelId(sw, ["do_open"])
    const doCloseId = resolveBindingChannelId(sw, ["do_closed"])
    if (!doOpenId || !doCloseId) return null

    const doOpen = params.channels.value.find(c => c.id === doOpenId)
    const doClose = params.channels.value.find(c => c.id === doCloseId)
    if (!doOpen || !doClose) return null
    if (doOpen.type !== CHANNEL_TYPES.DO || doClose.type !== CHANNEL_TYPES.DO) return null
    if (doOpen.device_id !== doClose.device_id) return null

    const unitId = params.resolveUnitId(doOpen.device_id)
    if (!unitId) return null

    return {
      unitId,
      chOpen: doOpen.index,
      chClose: doClose.index,
    }
  }

  function resolveSwitchgearState(sw: Switchgear): SwitchgearState {
    const doOpenChannelId = resolveBindingChannelId(sw, ["do_open"])
    const doCloseChannelId = resolveBindingChannelId(sw, ["do_closed"])
    if (!isChannelOnline(doOpenChannelId) || !isChannelOnline(doCloseChannelId)) {
      return "UNKNOWN"
    }

    const doOpen = resolveBinaryState(
      doOpenChannelId,
      CHANNEL_TYPES.DO,
    )
    const doClose = resolveBinaryState(
      doCloseChannelId,
      CHANNEL_TYPES.DO,
    )
    return resolvePairState(doOpen, doClose) ?? "UNKNOWN"
  }

  function isUnitOnline(sw: Switchgear): boolean {
    const chId = resolveBindingChannelId(sw, ["do_open", "do_closed"])
    if (!chId) return false

    const ch = params.channels.value.find(c => c.id === chId)
    if (!ch) return false

    const dev = params.devices.value.find(d => d.id === ch.device_id)
    return dev?.status === "online"
  }

  function pruneDiEdgeMemory(diEdgeMemory: Ref<Record<number, { diOpen: boolean | null; diClose: boolean | null }>>) {
    const activeIds = new Set(params.switchgears.value.map(sw => sw.id))
    for (const key of Object.keys(diEdgeMemory.value)) {
      const switchgearId = Number(key)
      if (!activeIds.has(switchgearId)) {
        delete diEdgeMemory.value[switchgearId]
      }
    }
  }

  return {
    bindingByRole,
    resolveBindingChannelId,
    resolveBinaryState,
    resolveTypedChannel,
    resolvePairState,
    resolveDoPair,
    resolveSwitchgearState,
    isUnitOnline,
    pruneDiEdgeMemory,
  }
}
