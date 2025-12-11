import { defineStore } from "pinia"
import { ref } from "vue"
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

  function resolveChannel(chId: number | null) {
    if (!chId) return null
    const channelStore = useChannelStore()
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
    loading.value = true
    try {
      const { data } = await SwitchgearsAPI.list()
      switchgears.value = data
      logger.info(`📡 Loaded ${data.length} switchgears`)
      loadedOnce.value = true

    } catch (err) {
      logger.error("💥 Failed to fetch switchgears:", err)
    } finally {
      loading.value = false
    }
  }

  async function ensureLoaded() {
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
      const { data } = await SwitchgearsAPI.create(body)
      switchgears.value.push(data)

      logger.info(`➕ Created switchgear id=${data.id}`)
      return data
    } catch (err) {
      logger.error("💥 Failed to create switchgear:", err)
      throw err
    }
  }

  async function createAuto() {
    const name = nextDefaultName()
    return await create({ name })
  }

  // Update single field(s)
  async function updateField(id: number, changes: SwitchgearUpdateInput) {
    try {

      const { data } = await SwitchgearsAPI.update(id, changes)
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
  }

  // Delete
  async function remove(id: number) {
    try {
      await SwitchgearsAPI.delete(id)
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
    if (!chId) return true

    const ch = useChannelStore().channels.find(c => c.id === chId)
    if (!ch) return true

    const dev = useDeviceStore().devices.find(d => d.id === ch.device_id)
    return dev?.status === "online"
  }

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
    bindingByRole,
    resolveBindingChannelId,
    duplicate,
    resetBindings,
  }
})
