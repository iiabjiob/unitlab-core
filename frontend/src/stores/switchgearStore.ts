import { defineStore } from "pinia"
import { ref } from "vue"
import type { Switchgear } from "@/types/switchgear"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import { getLogger } from "@/utils/logger"
import { useChannelStore } from "./channelStore"
import { useValidationStore } from "./validationStore"
import { validateSwitchgear } from "@/validators/switchgear"
import { switchgearPropertySchema } from "@/property-schemas/switchgear.schema"
import { SCHEMA_NAMES } from "@/property-schemas/types"
import { clearOne, validateOne } from "@/validators/syncValidation"
import { VALIDATION_LEVELS } from "@/validators/types"

const logger = getLogger("SG")

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
  async function create(payload: Omit<Switchgear, "id">) {
    try {
      const { data } = await axios.post<Switchgear>(ApiBuilder.switchgears(), payload)
      switchgears.value.push(data)

      validateOne(SCHEMA_NAMES.SWITCHGEAR, data, validateSwitchgear)

      logger.info(`➕ Created switchgear id=${data.id}`)
      return data
    } catch (err) {
      logger.error("💥 Failed to create switchgear:", err)
      throw err
    }
  }

  // Update single field(s)
  async function updateField(id: number, changes: Partial<Switchgear>) {
    try {
      // Pre-submit validation
      const current = switchgears.value.find(s => s.id === id)
      if (!current) return

      // создаём черновик: текущее + изменения
      const draft = { ...current, ...changes }
      const preErrors = validateSwitchgear(draft)

      validateOne(SCHEMA_NAMES.SWITCHGEAR, draft, validateSwitchgear)

      // если есть ошибки уровня error → не шлём запрос
      if (preErrors.some(e => e.level === VALIDATION_LEVELS.ERROR)) {
        logger.warn(`⚠️ Validation failed for switchgear ${id}`, preErrors)
        return
      }

      const { data } = await axios.patch<Switchgear>(ApiBuilder.switchgear(id), changes)
      const idx = switchgears.value.findIndex(s => s.id === id)
      if (idx !== -1) {
        switchgears.value[idx] = data

        validateOne(SCHEMA_NAMES.SWITCHGEAR, data, validateSwitchgear)
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

      clearOne(SCHEMA_NAMES.SWITCHGEAR, id)

      logger.info(`🗑️ Switchgear ${id} deleted`)
    } catch (err) {
      logger.error(`💥 Failed to delete switchgear ${id}:`, err)
    }
  }

  // Get by id (reactive helper)
  function getById(id: number) {
    return switchgears.value.find(s => s.id === id) ?? null
  }

  return {
    switchgears,
    resolveChannel,
    fetchAll,
    create,
    updateField,
    remove,
    getById,
  }
})
