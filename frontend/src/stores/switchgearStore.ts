import { defineStore } from "pinia"
import { ref, watch } from "vue"
import type {
  Switchgear,
  SwitchgearCreateInput,
  SwitchgearUpdateInput,
  SwitchgearBindingRole,
} from "@/types/switchgear"
import { SWITCHGEAR_BINDING_ROLES } from "@/types/switchgear"
import { SwitchgearsAPI } from "@/api/switchgears.api"
import { getLogger } from "@/utils/logger"
import { useChannelStore } from "./channelStore"
import { useDeviceStore } from "./deviceStore"
import { useWorkspaceStore } from "./workspaceStore"
import { CHANNEL_TYPES, type Channel, type ChannelType } from "@/types/channel"
import { codeToState, SWITCHGEAR_CODE, type SwitchgearState } from "@/constants/switchgear"

const logger = getLogger("SG")

function buildEmptyBindings() {
  return SWITCHGEAR_BINDING_ROLES.map(role => ({
    role,
    channel_id: null,
    delay_ms: 0,
  }))
}

type ChannelCandidate = {
  id: number
  index: number
  unitId: string
  onlineRank: number
}
type AutoBindingRole = "do_open" | "do_closed" | "di_open" | "di_close"

function normalizeUnitId(value: unknown): string {
  const normalized = String(value ?? "").trim()
  return normalized || "unknown"
}

export const useSwitchgearStore = defineStore("switchgearStore", () => {
  const switchgears = ref<Switchgear[]>([])
  const loading = ref(false)
  const loadedOnce = ref(false)
  const workspaceStore = useWorkspaceStore()
  const channelStore = useChannelStore()
  const deviceStore = useDeviceStore()
  const diEdgeMemory = ref<Record<number, { diOpen: boolean | null; diClose: boolean | null }>>({})
  const updateQueueById = new Map<number, Promise<void>>()
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

  function existingNames() {
    return new Set(switchgears.value.map(sw => sw.name))
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
    const deviceById = new Map(deviceStore.devices.map(device => [device.id, device] as const))
    return channelStore.channels
      .filter(channel => channel.type === type && !usedChannelIds.has(channel.id))
      .map((channel) => {
        const device = deviceById.get(channel.device_id)
        const unitId = normalizeUnitId(device?.unit_id ?? channelStore.resolveUnitId(channel.device_id))
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
    await Promise.all([
      deviceStore.ensureLoaded(),
      channelStore.ensureLoaded(),
    ])

    const usedChannelIds = new Set<number>()
    switchgears.value.forEach((switchgear) => {
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

    return buildEmptyBindings().map((binding) => ({
      ...binding,
      channel_id: byRole[binding.role as SwitchgearBindingRole] ?? null,
    }))
  }

  function resolveChannel(chId: number | null) {
    if (!chId) return null
    const ch = channelStore.channels.find(c => c.id === chId)
    if (!ch) return null
    return {
      unitId: channelStore.resolveUnitId(ch.device_id),
      channel: ch.index,
      type: ch.type,
    }
  }

  // Fetch all switchgears
  async function fetchAll() {
    if (!workspaceStore.activeWorkspaceId) return
    loading.value = true
    try {
      const workspaceId = workspaceStore.requireWorkspaceId()
      const { data } = await SwitchgearsAPI.list(workspaceId)
      switchgears.value = data
      logger.info(`📡 Loaded ${data.length} switchgears for workspace ${workspaceId}`)
      loadedOnce.value = true

    } catch (err) {
      logger.error("💥 Failed to fetch switchgears:", err)
    } finally {
      loading.value = false
    }
  }

  async function ensureLoaded() {
    if (!workspaceStore.activeWorkspaceId) {
      logger.debug("⏸️ No active workspace selected, skipping switchgear load")
      return
    }

    if (!loadedOnce.value && !loading.value) {
      await fetchAll()
    }
  }

  // Create new switchgear
  async function create(payload: SwitchgearCreateInput) {
    try {
      const body = {
        switchgear_type: payload.switchgear_type ?? "switchgear",
        name: payload.name,
        bindings: (payload.bindings && payload.bindings.length
          ? payload.bindings
          : buildEmptyBindings()).map(binding => ({
          delay_ms: 0,
          ...binding,
        })),
      }
      const { data } = await SwitchgearsAPI.create(workspaceStore.requireWorkspaceId(), body)
      switchgears.value.push(data)

      logger.info(`➕ Created switchgear id=${data.id}`)
      return data
    } catch (err) {
      logger.error("💥 Failed to create switchgear:", err)
      throw err
    }
  }

  async function createAuto() {
    await ensureLoaded()
    const name = nextDefaultName()
    const bindings = await buildAutoBindings()
    return await create({ name, bindings })
  }

  // Update single field(s)
  async function updateField(id: number, changes: SwitchgearUpdateInput) {
    const previous = updateQueueById.get(id) ?? Promise.resolve()
    const run = previous.catch(() => undefined).then(async () => {
      try {
        const { data } = await SwitchgearsAPI.update(workspaceStore.requireWorkspaceId(), id, changes)
        const idx = switchgears.value.findIndex(s => s.id === id)
        if (idx !== -1) {
          switchgears.value[idx] = data
        }
        logger.debug(`✏️ Switchgear ${id} updated`, changes)
        return data
      } catch (err) {
        logger.error(`💥 Failed to update switchgear ${id}:`, err)
        throw err
      }
    })
    updateQueueById.set(id, run.then(() => undefined, () => undefined))
    return await run
  }

  // Delete
  async function remove(id: number) {
    try {
      await SwitchgearsAPI.delete(workspaceStore.requireWorkspaceId(), id)
      switchgears.value = switchgears.value.filter(s => s.id !== id)

      logger.info(`🗑️ Switchgear ${id} deleted`)
    } catch (err) {
      logger.error(`💥 Failed to delete switchgear ${id}:`, err)
    }
  }

  // Get by id (reactive helper)
  function getById(id: number) {
    return switchgears.value.find(s => s.id === id) ?? null
  }

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

  function resolveBinaryState(
    channelId: number | null,
    expectedType: typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES],
  ): boolean | null {
    if (!channelId) return null
    const ch = channelStore.channels.find(c => c.id === channelId)
    if (!ch || ch.type !== expectedType) return null
    return typeof ch.state === "boolean" ? ch.state : null
  }

  function resolveTypedChannel(
    channelId: number | null,
    expectedType: typeof CHANNEL_TYPES[keyof typeof CHANNEL_TYPES],
  ): Channel | null {
    if (!channelId) return null
    const channel = channelStore.channels.find(item => item.id === channelId) ?? null
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

    const doOpen = channelStore.channels.find(c => c.id === doOpenId)
    const doClose = channelStore.channels.find(c => c.id === doCloseId)
    if (!doOpen || !doClose) return null
    if (doOpen.type !== CHANNEL_TYPES.DO || doClose.type !== CHANNEL_TYPES.DO) return null
    if (doOpen.device_id !== doClose.device_id) return null

    const unitId = channelStore.resolveUnitId(doOpen.device_id)
    if (!unitId) return null

    return {
      unitId,
      chOpen: doOpen.index,
      chClose: doClose.index,
    }
  }

  function syncDiDrivenSwitching() {
    const activeIds = new Set<number>()

    for (const sw of switchgears.value) {
      activeIds.add(sw.id)
      const diOpenChannel = resolveTypedChannel(
        resolveBindingChannelId(sw, ["di_open"]),
        CHANNEL_TYPES.DI,
      )
      const diCloseChannel = resolveTypedChannel(
        resolveBindingChannelId(sw, ["di_close"]),
        CHANNEL_TYPES.DI,
      )
      const diOpen = diOpenChannel && typeof diOpenChannel.state === "boolean"
        ? diOpenChannel.state
        : null
      const diClose = diCloseChannel && typeof diCloseChannel.state === "boolean"
        ? diCloseChannel.state
        : null

      const diChanged = Boolean(
        (diOpenChannel && channelStore.didChannelChangeInLastRevision(diOpenChannel.device_id, diOpenChannel.index)) ||
        (diCloseChannel && channelStore.didChannelChangeInLastRevision(diCloseChannel.device_id, diCloseChannel.index)),
      )
      if (!diChanged) {
        continue
      }

      const prev = diEdgeMemory.value[sw.id] ?? { diOpen: null, diClose: null }
      const openEdge = prev.diOpen === false && diOpen === true && diClose === false
      const closeEdge = prev.diClose === false && diClose === true && diOpen === false

      if (openEdge !== closeEdge) {
        const targetStateName: SwitchgearState = openEdge ? "OPEN" : "CLOSED"
        const feedbackDelayMsRaw = bindingByRole(sw, openEdge ? "di_open" : "di_close")?.delay_ms
        const feedbackDelayMs = Number.isFinite(feedbackDelayMsRaw as number)
          ? Math.max(0, Math.round(Number(feedbackDelayMsRaw)))
          : 0

        clearDiDelayTimer(sw.id)

        const runDiDrivenSwitch = () => {
          const latest = switchgears.value.find(item => item.id === sw.id)
          if (!latest) return

          const latestDiOpen = resolveBinaryState(
            resolveBindingChannelId(latest, ["di_open"]),
            CHANNEL_TYPES.DI,
          )
          const latestDiClose = resolveBinaryState(
            resolveBindingChannelId(latest, ["di_close"]),
            CHANNEL_TYPES.DI,
          )

          const targetStillActive = targetStateName === "OPEN"
            ? latestDiOpen === true && latestDiClose === false
            : latestDiClose === true && latestDiOpen === false

          if (!targetStillActive) {
            logger.debug(
              `Skip delayed DI-driven ${targetStateName} for switchgear ${sw.id}: DI state changed during feedback delay`,
            )
            return
          }

          const currentState = resolveSwitchgearState(latest)
          const doPair = resolveDoPair(latest)
          if (!doPair || currentState === targetStateName) {
            return
          }

          if (channelStore.hasPendingCommandForUnit(doPair.unitId)) {
            logger.debug(
              `Skip DI-driven ${targetStateName} for switchgear ${sw.id}: unit ${doPair.unitId} has pending command`,
            )
            return
          }

          const targetState = targetStateName === "OPEN" ? SWITCHGEAR_CODE.OPEN : SWITCHGEAR_CODE.CLOSED
          const result = channelStore.sendDoPairCommand(
            doPair.unitId,
            doPair.chOpen,
            doPair.chClose,
            targetState,
            { source: "switchgear-di-edge" },
          )

          if (!result.ok) {
            logger.warn(
              `DI edge failed for switchgear ${sw.id} -> ${targetStateName}: ${result.error}`,
            )
          } else {
            logger.debug(
              `DI edge drove switchgear ${sw.id} -> ${targetStateName} via ${doPair.unitId} [${doPair.chOpen}/${doPair.chClose}]`,
            )
          }
        }

        if (feedbackDelayMs > 0) {
          logger.debug(
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

      diEdgeMemory.value[sw.id] = { diOpen, diClose }
    }

    for (const key of Object.keys(diEdgeMemory.value)) {
      const switchgearId = Number(key)
      if (!activeIds.has(switchgearId)) {
        delete diEdgeMemory.value[switchgearId]
        clearDiDelayTimer(switchgearId)
      }
    }
  }

  function pruneDiEdgeMemory() {
    const activeIds = new Set(switchgears.value.map(sw => sw.id))
    for (const key of Object.keys(diEdgeMemory.value)) {
      const switchgearId = Number(key)
      if (!activeIds.has(switchgearId)) {
        delete diEdgeMemory.value[switchgearId]
      }
    }
  }

  function resolveSwitchgearState(sw: Switchgear): SwitchgearState {
    const doOpen = resolveBinaryState(
      resolveBindingChannelId(sw, ["do_open"]),
      CHANNEL_TYPES.DO,
    )
    const doClose = resolveBinaryState(
      resolveBindingChannelId(sw, ["do_closed"]),
      CHANNEL_TYPES.DO,
    )
    return resolvePairState(doOpen, doClose) ?? "UNKNOWN"
  }

  async function duplicate(id: number) {
    const original = switchgears.value.find(sw => sw.id === id)
    if (!original) {
      throw new Error(`Switchgear ${id} not found`)
    }

    const payload: SwitchgearCreateInput = {
      name: nextDuplicateName(original.name),
      switchgear_type: original.switchgear_type,
      bindings: original.bindings.map(binding => ({
        role: binding.role,
        channel_id: binding.channel_id,
        delay_ms: binding.delay_ms,
      })),
    }

    return await create(payload)
  }

  async function resetBindings(id: number) {
    const target = switchgears.value.find(sw => sw.id === id)
    if (!target) return
    await updateField(id, {
      bindings: buildEmptyBindings().map(binding => ({
        ...binding,
        channel_id: null,
        delay_ms: 0,
      })),
    })
  }

  function isUnitOnline(sw: Switchgear): boolean {
    const chId = resolveBindingChannelId(sw, ["do_open", "do_closed"])
    if (!chId) return false

    const ch = channelStore.channels.find(c => c.id === chId)
    if (!ch) return false

    const dev = deviceStore.devices.find(d => d.id === ch.device_id)
    return dev?.status === "online"
  }

  function resetForWorkspaceChange() {
    switchgears.value = []
    loadedOnce.value = false
    diEdgeMemory.value = {}
    updateQueueById.clear()
    clearAllDiDelayTimers()
  }

  watch(
    () => workspaceStore.activeWorkspaceId,
    (workspaceId) => {
      resetForWorkspaceChange()
      if (workspaceId) {
        void fetchAll()
      }
    },
  )

  watch(
    () => switchgears.value.map(sw => sw.id).join(","),
    () => {
      pruneDiEdgeMemory()
    },
    { immediate: true },
  )

  watch(
    () => channelStore.stateRevision,
    () => {
      syncDiDrivenSwitching()
    },
  )

  return {
    switchgears,
    loading,
    ensureLoaded,
    resolveChannel,
    fetchAll,
    create,
    createAuto,
    updateField,
    remove,
    getById,
    isUnitOnline,
    resolveSwitchgearState,
    bindingByRole,
    resolveBindingChannelId,
    duplicate,
    resetBindings,
  }
})
