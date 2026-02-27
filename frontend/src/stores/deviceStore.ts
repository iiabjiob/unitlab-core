import { defineStore } from "pinia"
import { ref, shallowRef } from "vue"

import { DevicesAPI } from "@/api/devices.api"
import type {
  Device,
  DeviceDto,
  DeviceStatus,
  DeviceBulkDeleteResponse,
  DeviceHeartbeatFastSnapshot,
  DeviceHeartbeatDiagSnapshot,
} from "@/types/device"
import type { Channel } from "@/types/channel"
import { ensureChannel } from "@/utils/channel"
import type { DeviceRegisterEvent, DeviceHeartbeatEvent } from "@/types/ws/events"
import { getLogger } from "@/utils/logger"
import { devPerfIncrement, devPerfMeasureStart } from "@/utils/devPerf"
import { useToastStore } from "@/stores/toastStore"
import router from "@/router"

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
    heartbeat_fast: dto.heartbeat_fast ?? undefined,
    heartbeat_diag: dto.heartbeat_diag ?? undefined,
  }
}

export const useDeviceStore = defineStore("deviceStore", () => {
  const devices = shallowRef<Device[]>([])
  const devicesRevision = ref(0)
  const isLoading = ref(false)
  const isLoaded = ref(false)
  const isDeleting = ref(false)
  const totalCount = ref(0)
  const toastStore = useToastStore()
  const offlineToastIdByUnitId = new Map<string, number>()
  let fetchAllInFlight: Promise<void> | null = null

  function bumpDevicesRevision() {
    devicesRevision.value += 1
  }

  function reset() {
    devices.value = []
    bumpDevicesRevision()
    isLoading.value = false
    isLoaded.value = false
    totalCount.value = 0
  }

  async function fetchAll() {
    devPerfIncrement("deviceStore.fetchAll.calls")
    if (fetchAllInFlight) {
      devPerfIncrement("deviceStore.fetchAll.dedupe_waits")
      await fetchAllInFlight
      return
    }

    const task = (async () => {
      const endMeasure = devPerfMeasureStart("deviceStore.fetchAll")
      isLoading.value = true
      try {
        const { data } = await DevicesAPI.list()
        const normalized = data.map(normalizeDevice)
        devices.value = normalized
        bumpDevicesRevision()
        totalCount.value = normalized.length
        isLoaded.value = true
        devPerfIncrement("deviceStore.fetchAll.completed")
        endMeasure({ count: normalized.length })
      } catch (err) {
        logger.error("Failed to fetch devices", err)
        endMeasure({ error: true })
        throw err
      } finally {
        isLoading.value = false
      }
    })()

    fetchAllInFlight = task
    try {
      await task
    } finally {
      if (fetchAllInFlight === task) {
        fetchAllInFlight = null
      }
    }
  }

  async function ensureLoaded() {
    devPerfIncrement("deviceStore.ensureLoaded.calls")
    if (!isLoaded.value) {
      devPerfIncrement("deviceStore.ensureLoaded.fetches")
      await fetchAll()
      return
    }
    devPerfIncrement("deviceStore.ensureLoaded.cache_hits")
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
        bumpDevicesRevision()
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
      bumpDevicesRevision()
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
    bumpDevicesRevision()

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
      bumpDevicesRevision()
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
    const idx = devices.value.findIndex(d => d.id === dto.id)
    const previous = idx >= 0 ? devices.value[idx] : null
    const updated = normalizeDevice({
      ...dto,
      heartbeat_fast: dto.heartbeat_fast ?? previous?.heartbeat_fast ?? null,
      heartbeat_diag: dto.heartbeat_diag ?? previous?.heartbeat_diag ?? null,
    })

    if (idx === -1) {
      devices.value = [...devices.value, updated]
    } else {
      const next = [...devices.value]
      next[idx] = updated
      devices.value = next
    }
    bumpDevicesRevision()
  }

  function setStatus(
    unitId: string,
    status: DeviceStatus,
    lastSeen?: number | null,
    telemetry?: {
      heartbeat_fast?: DeviceHeartbeatFastSnapshot | null
      heartbeat_diag?: DeviceHeartbeatDiagSnapshot | null
    },
  ) {
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
      heartbeat_fast: telemetry?.heartbeat_fast ?? dev.heartbeat_fast ?? null,
      heartbeat_diag: telemetry?.heartbeat_diag ?? dev.heartbeat_diag ?? null,
    }

    const next = normalizeDevice(dto)
    const copy = [...devices.value]
    copy[idx] = next
    devices.value = copy
    bumpDevicesRevision()
  }

  async function toggleDeviceActive(deviceId: number) {
    logger.warn(`toggleDeviceActive is deprecated in the new devices API (deviceId=${deviceId})`)
  }

  function updateStatus(event: DeviceHeartbeatEvent) {
    const existing = devices.value.find(d => d.unit_id === event.unit_id)
    const previousStatus = existing?.status

    setStatus(event.unit_id, event.status, event.last_seen, {
      heartbeat_fast: event.heartbeat_fast ?? undefined,
      heartbeat_diag: event.heartbeat_diag ?? undefined,
    })

    if (!previousStatus || previousStatus === event.status) {
      return
    }

    const displayName = existing?.display_name ?? event.unit_id
    if (event.status === "online") {
      const staleOfflineToastId = offlineToastIdByUnitId.get(event.unit_id)
      if (staleOfflineToastId !== undefined) {
        toastStore.remove(staleOfflineToastId)
        offlineToastIdByUnitId.delete(event.unit_id)
      }
      toastStore.success(`${displayName} is back online`)
      return
    }

    const staleOfflineToastId = offlineToastIdByUnitId.get(event.unit_id)
    if (staleOfflineToastId !== undefined) {
      toastStore.remove(staleOfflineToastId)
      offlineToastIdByUnitId.delete(event.unit_id)
    }
    const toastId = toastStore.error(`${displayName} went offline`, {
      timeout: null,
      actionLabel: "Open Devices",
      onAction: () => {
        void router.push({ name: "devices.list" }).catch(() => undefined)
      },
    })
    offlineToastIdByUnitId.set(event.unit_id, toastId)
  }

  return {
    devices,
    devicesRevision,
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
