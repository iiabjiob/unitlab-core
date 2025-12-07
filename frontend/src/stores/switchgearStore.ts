import { defineStore } from "pinia"
import { ref } from "vue"
import type {
  Switchgear,
  SwitchgearCreateInput,
  SwitchgearUpdateInput,
  SwitchgearBindingRole,
} from "@/types/switchgear"
import { SWITCHGEAR_BINDING_ROLES } from "@/types/switchgear"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
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
    try {
      const { data } = await axios.get<Switchgear[]>(ApiBuilder.switchgears())
      switchgears.value = data
      logger.info(`📡 Loaded ${data.length} switchgears`)

    } catch (err) {
      logger.error("💥 Failed to fetch switchgears:", err)
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
      const { data } = await axios.post<Switchgear>(ApiBuilder.switchgears(), body)
      switchgears.value.push(data)

      logger.info(`➕ Created switchgear id=${data.id}`)
      return data
    } catch (err) {
      logger.error("💥 Failed to create switchgear:", err)
      throw err
    }
  }

  // Update single field(s)
  async function updateField(id: number, changes: SwitchgearUpdateInput) {
    try {

      const { data } = await axios.patch<Switchgear>(ApiBuilder.switchgear(id), changes)
      const idx = switchgears.value.findIndex(s => s.id === id)
      if (idx !== -1) {
        switchgears.value[idx] = data
      }
      logger.debug(`✏️ Switchgear ${id} updated`, changes)
    } catch (err) {
      logger.error(`💥 Failed to update switchgear ${id}:`, err)
    }
  }

  // Delete
  async function remove(id: number) {
    try {
      await axios.delete(ApiBuilder.switchgear(id))
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
    resolveChannel,
    fetchAll,
    create,
    updateField,
    remove,
    getById,
    isUnitOnline,
    bindingByRole,
  }
})
