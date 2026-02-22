import type { Ref } from "vue"

import { CHANNEL_TYPES, type Channel, type ChannelType } from "@/types/channel"
import type { Switchgear } from "@/types/switchgear"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

type DeviceLike = {
  id: number
  unit_id?: string | null
  status?: string | null
}

type BindingTemplate = {
  role: string
  channel_id: number | null
  delay_ms: number
}

type LoggerLike = {
  debug: (message: string, ...args: unknown[]) => void
}

type ChannelCandidate = {
  id: number
  index: number
  unitId: string
  onlineRank: number
}

type AutoBindingRole = "do_open" | "do_closed" | "di_open" | "di_close"

type Params = {
  logger: LoggerLike
  switchgears: Ref<Switchgear[]>
  devices: Ref<DeviceLike[]>
  channels: Ref<Channel[]>
  getActiveWorkspaceId: () => number | null
  ensureDevicesLoaded: () => Promise<void>
  ensureChannelsLoaded: () => Promise<void>
  resolveUnitId: (deviceId: number) => string
  buildEmptyBindings: () => BindingTemplate[]
}

function normalizeUnitId(value: unknown): string {
  const normalized = String(value ?? "").trim()
  return normalized || "unknown"
}

function isAutoBindingRole(role: string): role is AutoBindingRole {
  return role === "do_open" || role === "do_closed" || role === "di_open" || role === "di_close"
}

export function createSwitchgearAutoBindings(params: Params) {
  function existingNames() {
    return new Set(params.switchgears.value.map(sw => sw.name))
  }

  function nextDefaultName() {
    const base = "New Switchgear"
    const names = existingNames()
    let suffix = 1
    while (names.has(`${base} ${suffix}`)) {
      suffix += 1
    }
    return `${base} ${suffix}`
  }

  function nextDuplicateName(sourceName: string) {
    const trimmed = sourceName.trim() || "Switchgear"
    const names = existingNames()
    let candidate = `${trimmed} copy`
    let counter = 2
    while (names.has(candidate)) {
      candidate = `${trimmed} copy ${counter}`
      counter += 1
    }
    return candidate
  }

  function availableCandidatesByType(
    type: ChannelType,
    usedChannelIds: Set<number>,
  ): ChannelCandidate[] {
    const deviceById = new Map(params.devices.value.map(device => [device.id, device] as const))
    return params.channels.value
      .filter(channel => channel.type === type && !usedChannelIds.has(channel.id))
      .map((channel) => {
        const device = deviceById.get(channel.device_id)
        const unitId = normalizeUnitId(device?.unit_id ?? params.resolveUnitId(channel.device_id))
        return {
          id: channel.id,
          index: channel.index,
          unitId,
          onlineRank: device?.status === "online" ? 0 : 1,
        }
      })
      .sort((a, b) => {
        if (a.onlineRank !== b.onlineRank) return a.onlineRank - b.onlineRank
        const byUnit = a.unitId.localeCompare(b.unitId)
        if (byUnit !== 0) return byUnit
        return a.index - b.index
      })
  }

  function pickPreferredChannels(
    candidates: ChannelCandidate[],
    count: number,
  ): ChannelCandidate[] {
    if (count <= 0 || !candidates.length) return []

    const grouped = new Map<string, ChannelCandidate[]>()
    candidates.forEach((candidate) => {
      const bucket = grouped.get(candidate.unitId)
      if (bucket) {
        bucket.push(candidate)
      } else {
        grouped.set(candidate.unitId, [candidate])
      }
    })

    for (const [, bucket] of grouped) {
      if (bucket.length >= count) {
        return bucket.slice(0, count)
      }
    }
    return candidates.slice(0, count)
  }

  async function buildAutoBindings() {
    await runStoreBootstrap(
      ["switchgear-auto-bindings", params.getActiveWorkspaceId()],
      [
        () => params.ensureDevicesLoaded(),
        () => params.ensureChannelsLoaded(),
      ],
      { mode: "settled" },
    )

    const usedChannelIds = new Set<number>()
    params.switchgears.value.forEach((switchgear) => {
      switchgear.bindings.forEach((binding) => {
        if (Number.isFinite(binding.channel_id as number)) {
          usedChannelIds.add(Number(binding.channel_id))
        }
      })
    })

    const doCandidates = availableCandidatesByType(CHANNEL_TYPES.DO, usedChannelIds)
    const selectedDo = pickPreferredChannels(doCandidates, 2)
    selectedDo.forEach((channel) => usedChannelIds.add(channel.id))

    const diCandidates = availableCandidatesByType(CHANNEL_TYPES.DI, usedChannelIds)
    const selectedDi = pickPreferredChannels(diCandidates, 2)
    selectedDi.forEach((channel) => usedChannelIds.add(channel.id))

    const byRole: Record<AutoBindingRole, number | null> = {
      do_open: selectedDo[0]?.id ?? null,
      do_closed: selectedDo[1]?.id ?? null,
      di_open: selectedDi[0]?.id ?? null,
      di_close: selectedDi[1]?.id ?? null,
    }

    params.logger.debug("Built switchgear auto-bindings", { byRole })

    return params.buildEmptyBindings().map((binding) => ({
      ...binding,
      channel_id: isAutoBindingRole(binding.role) ? (byRole[binding.role] ?? null) : null,
    }))
  }

  return {
    nextDefaultName,
    nextDuplicateName,
    buildAutoBindings,
  }
}
