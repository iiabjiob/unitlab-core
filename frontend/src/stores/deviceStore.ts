import { defineStore } from "pinia"
import { ref, shallowRef } from "vue"

import { DevicesAPI } from "@/api/devices.api"
import type {
  Device,
  DeviceDto,
  DeviceStatus,
  DeviceBulkDeleteResponse,
} from "@/types/device"
import type { Channel } from "@/types/channel"
import { ensureChannel } from "@/utils/channel"
import type { DeviceRegisterEvent, DeviceHeartbeatEvent } from "@/types/ws/events"
import { getLogger } from "@/utils/logger"
import { useToastStore } from "@/stores/toastStore"

const logger = getLogger("DEVICE")

function normalizeChannels(channels: DeviceDto["channels"], fallbackType?: DeviceDto["device_type"] | null): Channel[] {
  if (!channels) {
    return []
  }

  return channels.map(channel => ensureChannel(channel, fallbackType ?? null))
}

function normalizeDevice(dto: DeviceDto): Device {
  const status: DeviceStatus = dto.status === "online" ? "online" : "offline"
  const channels = normalizeChannels(dto.channels, dto.device_type)
  const numChannels = dto.num_channels ?? (channels?.length ?? 0)

  return {
    id: dto.id,
    unit_id: dto.unit_id,
    device_type: dto.device_type,
    display_name: dto.name?.trim() || dto.unit_id,
    name: dto.name ?? null,
    num_channels: numChannels,
    channels,
    firmware_version: dto.firmware_version ?? undefined,
    online: status === "online",
    status,
    last_seen: dto.last_seen ?? undefined,
    registered_at: dto.registered_at ?? undefined,
    type: dto.device_type,
    is_active: true,
    location: undefined,
  }
}

export const useDeviceStore = defineStore("deviceStore", () => {
  const devices = shallowRef<Device[]>([])
  const isLoading = ref(false)
  const isLoaded = ref(false)
  const isDeleting = ref(false)
  const totalCount = ref(0)
  const toastStore = useToastStore()

  function reset() {
    devices.value = []
    isLoading.value = false
    isLoaded.value = false
    totalCount.value = 0
  }

  async function fetchAll() {
    isLoading.value = true
    try {
      const { data } = await DevicesAPI.list()
      const normalized = data.map(normalizeDevice)
      devices.value = normalized
      totalCount.value = normalized.length
      isLoaded.value = true
    } catch (err) {
      logger.error("Failed to fetch devices", err)
      throw err
    } finally {
      isLoading.value = false
    }
  }

  async function ensureLoaded() {
    if (!isLoaded.value) {
      await fetchAll()
    }
  }

  async function updateDeviceField(deviceId: number, changes: Partial<DeviceDto>) {
    try {
      const { data } = await DevicesAPI.update(deviceId, changes)
      const updated = normalizeDevice(data)

      const index = devices.value.findIndex(d => d.id === deviceId)
      if (index !== -1) {
        const next = [...devices.value]
        next[index] = updated
        devices.value = next
      }

      logger.debug(`Device ${deviceId} updated`, updated)
    } catch (err) {
      logger.error(`Failed to update device ${deviceId}`, err)
    }
  }

  async function deleteDevice(deviceId: number) {
    try {
      await DevicesAPI.delete(deviceId)
      devices.value = devices.value.filter(d => d.id !== deviceId)
      totalCount.value = devices.value.length
    } catch (error) {
      logger.error("Failed to delete device", error)
    }
  }

  async function deleteMany(ids: number[]) {
    const uniqueIds = Array.from(new Set(ids)).filter(id => Number.isInteger(id))
    if (!uniqueIds.length) {
      return { requested: 0, deleted: 0 }
    }
    if (isDeleting.value) {
      return { requested: uniqueIds.length, deleted: 0 }
    }

    isDeleting.value = true
    const previous = [...devices.value]
    const removeSet = new Set(uniqueIds)
    devices.value = devices.value.filter(d => !removeSet.has(d.id))

    try {
      const { data } = await DevicesAPI.bulkDelete(uniqueIds)
      const deleted = data?.deleted ?? 0
      if (deleted !== uniqueIds.length) {
        logger.warn(`Backend mismatch: requested=${uniqueIds.length}, deleted=${deleted}. Refetching...`)
        await fetchAll()
      } else {
        totalCount.value = devices.value.length
      }
      return { requested: uniqueIds.length, deleted }
    } catch (err) {
      devices.value = previous
      logger.error("Failed to delete devices", err)
      throw err
    } finally {
      isDeleting.value = false
    }
  }

  function fromRegisterEvent(ev: DeviceRegisterEvent): Device {
    return normalizeDevice({
      id: ev.id,
      unit_id: ev.unit_id,
      device_type: ev.device_type,
      num_channels: ev.num_channels ?? 0,
      firmware_version: ev.firmware_version ?? null,
      name: ev.name ?? null,
      status: ev.status,
      last_seen: ev.last_seen ?? null,
      registered_at: ev.registered_at ?? null,
      channels: ev.channels ?? null,
    })
  }

  function upsertDevice(dto: DeviceDto) {
    const updated = normalizeDevice(dto)
    const idx = devices.value.findIndex(d => d.id === updated.id)

    if (idx === -1) {
      devices.value = [...devices.value, updated]
    } else {
      const next = [...devices.value]
      next[idx] = updated
      devices.value = next
    }
  }

  function setStatus(unitId: string, status: DeviceStatus, lastSeen?: number | null) {
    const idx = devices.value.findIndex(d => d.unit_id === unitId)
    if (idx === -1) return

    const dev = devices.value[idx]
    const dto: DeviceDto = {
      id: dev.id,
      unit_id: dev.unit_id,
      device_type: dev.device_type,
      name: dev.name ?? null,
      status,
      num_channels: dev.num_channels,
      firmware_version: dev.firmware_version ?? null,
      last_seen: lastSeen ?? dev.last_seen ?? null,
      registered_at: dev.registered_at ?? null,
      channels: dev.channels,
    }

    const next = normalizeDevice(dto)
    const copy = [...devices.value]
    copy[idx] = next
    devices.value = copy
  }

  async function toggleDeviceActive(deviceId: number) {
    logger.warn(`toggleDeviceActive is deprecated in the new devices API (deviceId=${deviceId})`)
  }

  function updateStatus(event: DeviceHeartbeatEvent) {
    const existing = devices.value.find(d => d.unit_id === event.unit_id)
    const previousStatus = existing?.status

    setStatus(event.unit_id, event.status, event.last_seen)

    if (!previousStatus || previousStatus === event.status) {
      return
    }

    const displayName = existing?.display_name ?? event.unit_id
    const variant = event.status === "online" ? "success" : "error"
    const message =
      event.status === "online"
        ? `${displayName} is back online`
        : `${displayName} went offline`

    toastStore.push(message, { variant })
  }

  return {
    devices,
    isLoading,
    isLoaded,
    isDeleting,
    totalCount,
    reset,
    fetchAll,
    ensureLoaded,
    updateDeviceField,
    deleteDevice,
    deleteMany,
    upsertDevice,
    fromRegisterEvent,
    setStatus,
    updateStatus,
    toggleDeviceActive,
  }
})
