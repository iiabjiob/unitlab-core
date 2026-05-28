import { defineStore, storeToRefs } from "pinia"
import { ref } from "vue"
import type {
  Switchgear,
  SwitchgearCreateInput,
} from "@/types/switchgear"
import { SWITCHGEAR_BINDING_ROLES } from "@/types/switchgear"
import { getLogger } from "@/utils/logger"
import { useChannelStore } from "./channelStore"
import { useDeviceStore } from "./deviceStore"
import { useWorkspaceStore } from "./workspaceStore"
import { createSwitchgearRuntimeResolvers } from "@/stores/switchgearStore/runtimeResolvers"
import { createSwitchgearDiEdgeSync } from "@/stores/switchgearStore/diEdgeSync"
import { createSwitchgearCatalogCrud } from "@/stores/switchgearStore/catalogCrud"
import { createSwitchgearAutoBindings } from "@/stores/switchgearStore/autoBindings"
import { createSwitchgearLifecycle } from "@/stores/switchgearStore/lifecycle"

const logger = getLogger("SG")

function buildEmptyBindings() {
  return SWITCHGEAR_BINDING_ROLES.map(role => ({
    role,
    channel_id: null,
    delay_ms: 0,
  }))
}

export const useSwitchgearStore = defineStore("switchgearStore", () => {
  const switchgears = ref<Switchgear[]>([])
  const loading = ref(false)
  const loadedOnce = ref(false)
  const workspaceStore = useWorkspaceStore()
  const channelStore = useChannelStore()
  const deviceStore = useDeviceStore()
  const { channels } = storeToRefs(channelStore)
  const { devices } = storeToRefs(deviceStore)
  const diEdgeMemory = ref<Record<number, { diOpen: boolean | null; diClose: boolean | null }>>({})
  const updateQueueById = new Map<number, Promise<void>>()

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

  const runtimeResolvers = createSwitchgearRuntimeResolvers({
    switchgears,
    channels,
    devices,
    resolveUnitId: channelStore.resolveUnitId,
  })
  const {
    bindingByRole,
    resolveBindingChannelId,
    resolveBinaryState,
    resolveTypedChannel,
    resolveDoPair,
    resolveSwitchgearState,
    isUnitOnline,
    pruneDiEdgeMemory,
  } = runtimeResolvers

  const diEdgeSync = createSwitchgearDiEdgeSync({
    logger,
    switchgears,
    diEdgeMemory,
    bindingByRole,
    resolveBindingChannelId,
    resolveBinaryState,
    resolveTypedChannel,
    resolveSwitchgearState,
    resolveDoPair,
    didChannelChangeInLastRevision: channelStore.didChannelChangeInLastRevision,
    hasPendingCommandForUnit: channelStore.hasPendingCommandForUnit,
    sendDoPairCommand: channelStore.sendDoPairCommand,
  })
  const {
    clearAllDiDelayTimers,
    syncDiDrivenSwitching,
  } = diEdgeSync
  const autoBindings = createSwitchgearAutoBindings({
    logger,
    switchgears,
    devices,
    channels,
    getActiveWorkspaceId: () => workspaceStore.activeWorkspaceId,
    ensureDevicesLoaded: () => deviceStore.ensureLoaded(),
    ensureChannelsLoaded: () => channelStore.ensureLoaded(),
    resolveUnitId: channelStore.resolveUnitId,
    buildEmptyBindings,
  })
  const {
    nextDefaultName,
    nextDuplicateName,
    buildAutoBindings,
  } = autoBindings
  const catalogCrud = createSwitchgearCatalogCrud({
    logger,
    switchgears,
    loading,
    loadedOnce,
    updateQueueById,
    getActiveWorkspaceId: () => workspaceStore.activeWorkspaceId,
    requireWorkspaceId: () => workspaceStore.requireWorkspaceId(),
    buildEmptyBindings,
  })
  const {
    fetchAll,
    ensureLoaded,
    create,
    updateField,
    remove,
    removeMany,
  } = catalogCrud

  async function createAuto() {
    await ensureLoaded()
    const name = nextDefaultName()
    const bindings = await buildAutoBindings()
    return await create({ name, bindings })
  }

  // Get by id (reactive helper)
  function getById(id: number) {
    return switchgears.value.find(s => s.id === id) ?? null
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

  createSwitchgearLifecycle({
    switchgears,
    loadedOnce,
    diEdgeMemory,
    updateQueueById,
    getActiveWorkspaceId: () => workspaceStore.activeWorkspaceId,
    getChannelStateRevision: () => channelStore.stateRevision,
    clearAllDiDelayTimers,
    fetchAll,
    pruneDiEdgeMemory,
    syncDiDrivenSwitching,
  })

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
    removeMany,
    getById,
    isUnitOnline,
    resolveSwitchgearState,
    bindingByRole,
    resolveBindingChannelId,
    duplicate,
    resetBindings,
  }
})
