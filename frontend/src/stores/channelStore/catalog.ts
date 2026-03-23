import type { Ref } from "vue"

import { ChannelsAPI } from "@/api/channels.api"
import { DevicesAPI } from "@/api/devices.api"
import { normalizeChannel, ensureChannel } from "@/utils/channel"
import { CHANNEL_TYPES, type AoChannel, type Channel, type ChannelDto, type ChannelListDto, type ChannelType, type DiChannel, type DoChannel, type DoChannelUiState } from "@/types/channel"

type LoggerLike = {
  info: (message: string) => void
  debug: (message: string, ...args: unknown[]) => void
  error: (message: string, ...args: unknown[]) => void
}

type DeviceLike = {
  id: number
  device_type: string
}

type Params = {
  channels: Ref<Channel[]>
  isLoading: Ref<boolean>
  isLoaded: Ref<boolean>
  logger: LoggerLike
  channelKey: (deviceId: number, chIndex: number) => string
  findDeviceById: (deviceId: number) => DeviceLike | undefined
  clearDoUiTimers: (ui: DoChannelUiState) => void
}

export function createChannelCatalog(params: Params) {
  const deviceLoading = new Set<number>()
  const deviceLoaded = new Set<number>()
  const channelsIndexByDevice = new Map<number, Channel[]>()
  const channelsIndexByDeviceAndChannel = new Map<string, Channel>()
  const EMPTY_CHANNELS: readonly Channel[] = []

  function normalizeDeviceChannelType(raw: unknown): ChannelType | null {
    const value = String(raw ?? "").trim().toLowerCase()
    if (value === CHANNEL_TYPES.DI) return CHANNEL_TYPES.DI
    if (value === CHANNEL_TYPES.DO) return CHANNEL_TYPES.DO
    if (value === CHANNEL_TYPES.AO) return CHANNEL_TYPES.AO
    return null
  }

  function rebuildChannelIndexes() {
    channelsIndexByDevice.clear()
    channelsIndexByDeviceAndChannel.clear()
    for (const channel of params.channels.value) {
      const byDevice = channelsIndexByDevice.get(channel.device_id)
      if (byDevice) {
        byDevice.push(channel)
      } else {
        channelsIndexByDevice.set(channel.device_id, [channel])
      }
      channelsIndexByDeviceAndChannel.set(params.channelKey(channel.device_id, channel.index), channel)
    }

    for (const byDevice of channelsIndexByDevice.values()) {
      byDevice.sort((left, right) => {
        if (left.index !== right.index) {
          return left.index - right.index
        }
        return left.id - right.id
      })
    }
  }

  function channelsByDeviceFast(deviceId: number): readonly Channel[] {
    return channelsIndexByDevice.get(deviceId) ?? EMPTY_CHANNELS
  }

  function channelByDeviceAndIndex(deviceId: number, chIndex: number): Channel | undefined {
    return channelsIndexByDeviceAndChannel.get(params.channelKey(deviceId, chIndex))
  }

  function preserveRuntimeState(next: Channel, previous?: Channel): Channel {
    if (!previous || previous.type !== next.type) {
      return next
    }

    if (next.type === CHANNEL_TYPES.DO && previous.type === CHANNEL_TYPES.DO) {
      const nextDo = next as DoChannel
      const previousDo = previous as DoChannel
      return {
        ...nextDo,
        state: previousDo.state,
        diagnostics: previousDo.diagnostics ? { ...previousDo.diagnostics } : undefined,
      }
    }

    if (next.type === CHANNEL_TYPES.DI && previous.type === CHANNEL_TYPES.DI) {
      const nextDi = next as DiChannel
      const previousDi = previous as DiChannel
      return {
        ...nextDi,
        state: previousDi.state,
        diDiagnostics: previousDi.diDiagnostics ? { ...previousDi.diDiagnostics } : undefined,
      }
    }

    if (next.type === CHANNEL_TYPES.AO && previous.type === CHANNEL_TYPES.AO) {
      const nextAo = next as AoChannel
      const previousAo = previous as AoChannel
      return {
        ...nextAo,
        state: previousAo.state,
        diagnostics: previousAo.diagnostics ? { ...previousAo.diagnostics } : undefined,
      }
    }

    return next
  }

  function applyInitialChannels(deviceId: number, list: Channel[]) {
    const next: Channel[] = []
    for (const ch of params.channels.value) {
      if (ch.device_id !== deviceId) {
        next.push(ch)
        continue
      }
      if (ch.type === CHANNEL_TYPES.DO && ch.ui) {
        params.clearDoUiTimers(ch.ui)
      }
    }
    for (const ch of list) {
      next.push(ch)
    }
    params.channels.value = next
    rebuildChannelIndexes()
  }

  function setBaseChannels(deviceId: number, base: Array<Channel | ChannelDto>) {
    const fallbackType = normalizeDeviceChannelType(params.findDeviceById(deviceId)?.device_type)
    const existingByIndex = new Map<number, Channel>()
    for (const channel of channelsByDeviceFast(deviceId)) {
      existingByIndex.set(channel.index, channel)
    }
    const prepared = base.map((raw) => {
      const normalized = ensureChannel(raw, fallbackType)
      const next = {
        ...normalized,
        device_id: deviceId,
      }
      return preserveRuntimeState(next, existingByIndex.get(next.index))
    })
    applyInitialChannels(deviceId, prepared)
    deviceLoaded.add(deviceId)
    params.logger.debug(`📡 Base channels set for ${deviceId} (${prepared.length})`)
  }

  async function fetchAll() {
    params.isLoading.value = true
    try {
      const { data } = await ChannelsAPI.list()
      const payload = Array.isArray(data)
        ? data
        : (Array.isArray((data as ChannelListDto).items)
          ? (data as ChannelListDto).items
          : [])
      const existingByDeviceAndIndex = new Map<string, Channel>()
      params.channels.value.forEach((channel) => {
        existingByDeviceAndIndex.set(params.channelKey(channel.device_id, channel.index), channel)
      })

      params.channels.value = payload
        .map(dto => normalizeChannel(dto))
        .map((next) => {
          const previous = existingByDeviceAndIndex.get(params.channelKey(next.device_id, next.index))
          return preserveRuntimeState(next, previous)
        })
      rebuildChannelIndexes()
      params.isLoaded.value = true
      params.logger.info(`📡 Loaded ${payload.length} channels`)
    } catch (err) {
      params.logger.error("Failed to load channels", err)
      throw err
    } finally {
      params.isLoading.value = false
    }
  }

  async function ensureLoaded() {
    if ((!params.isLoaded.value || params.channels.value.length === 0) && !params.isLoading.value) {
      await fetchAll()
    }
  }

  async function fetchByDevice(deviceId: number, force = false) {
    if (!Number.isFinite(deviceId) || deviceId <= 0) {
      return
    }
    if (!force && deviceLoaded.has(deviceId)) {
      return
    }
    if (deviceLoading.has(deviceId)) {
      return
    }

    deviceLoading.add(deviceId)
    try {
      const { data } = await DevicesAPI.getChannels(deviceId, { limit: 1000, offset: 0 })
      const payload = Array.isArray(data)
        ? data
        : (Array.isArray((data as ChannelListDto).items)
          ? (data as ChannelListDto).items
          : [])
      setBaseChannels(deviceId, payload)
      deviceLoaded.add(deviceId)
    } catch (err) {
      params.logger.error(`Failed to load channels for device ${deviceId}`, err)
      throw err
    } finally {
      deviceLoading.delete(deviceId)
    }
  }

  async function ensureDeviceChannelsLoaded(deviceId: number) {
    await fetchByDevice(deviceId, false)
  }

  function invalidateDeviceChannels(deviceId: number) {
    deviceLoaded.delete(deviceId)
  }

  function channelsByDevice(deviceId: number) {
    void params.channels.value.length
    return [...channelsByDeviceFast(deviceId)]
  }

  function hasDeviceChannels(deviceId: number): boolean {
    return channelsByDeviceFast(deviceId).length > 0
  }

  async function updateChannelField(id: number, changes: Partial<ChannelDto>) {
    try {
      const { data } = await ChannelsAPI.update(id, changes)
      const updated = normalizeChannel(data)
      const idx = params.channels.value.findIndex(c => c.id === id)
      if (idx !== -1) {
        const current = params.channels.value[idx]
        const needsReindex =
          current.device_id !== updated.device_id ||
          current.index !== updated.index ||
          current.type !== updated.type

        Object.assign(current, updated)

        if (needsReindex) {
          rebuildChannelIndexes()
        }
      }
      params.logger.debug(`Channel ${id} updated`, updated)
      return updated
    } catch (error) {
      params.logger.error(`Failed to update channel ${id}`, error)
      throw error
    }
  }

  function resetCatalog() {
    params.channels.value.forEach(ch => {
      if (ch.type === CHANNEL_TYPES.DO && ch.ui) {
        params.clearDoUiTimers(ch.ui)
      }
    })
    params.channels.value = []
    rebuildChannelIndexes()
    params.isLoaded.value = false
    deviceLoaded.clear()
    deviceLoading.clear()
  }

  return {
    rebuildChannelIndexes,
    channelsByDeviceFast,
    channelByDeviceAndIndex,
    fetchAll,
    ensureLoaded,
    fetchByDevice,
    ensureDeviceChannelsLoaded,
    invalidateDeviceChannels,
    channelsByDevice,
    hasDeviceChannels,
    updateChannelField,
    setBaseChannels,
    resetCatalog,
  }
}
