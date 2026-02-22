import { computed, onBeforeUnmount, watch, type ComputedRef, type Ref } from "vue"

import type { Channel } from "@/types/channel"
import type { Device } from "@/types/device"
import type { SignalAllocationRow } from "@/types/signal"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"

type ChannelStoreLike = Pick<ReturnType<typeof useChannelStore>,
  "hasDeviceChannels" | "ensureDeviceChannelsLoaded" | "requestStates">

type DeviceStoreLike = Pick<ReturnType<typeof useDeviceStore>, "devices">
type RealtimeScopeStoreLike = Pick<ReturnType<typeof useRealtimeScopeStore>,
  "setRealtimeUnitScope" | "clearRealtimeUnitScope">

type Params = {
  scopeId: string
  allocationRows: Ref<SignalAllocationRow[]>
  allocationRevision: Ref<number>
  channels: Ref<Channel[]>
  devicesRevision: Ref<number>
  activeWorkspaceId: ComputedRef<number | null>
  isTestRunBusy: ComputedRef<boolean>
  isWsConnected: Ref<boolean>
  channelMap: ComputedRef<Map<number, Channel>>
  channelUnitById: ComputedRef<Map<number, string>>
  deviceStore: DeviceStoreLike
  channelStore: ChannelStoreLike
  realtimeScopeStore: RealtimeScopeStoreLike
}

export function useSignalsRealtimeUnitScope(params: Params) {
  let realtimeScopeSyncFrame: number | null = null
  let lastRealtimeScopeKey = ""
  let wsScopeRetryTimer: ReturnType<typeof setTimeout> | null = null
  const realtimeScopeChannelHydrationInFlight = new Set<number>()

  function syncRealtimeUnitScope() {
    const units = new Set<string>()
    params.allocationRows.value.forEach((row) => {
      if (!Number.isFinite(row.channel_id as number) || Number(row.channel_id) <= 0) return
      const channel = params.channelMap.value.get(Number(row.channel_id))
      const unitId = channel
        ? (params.channelUnitById.value.get(channel.id) ?? null)
        : String(row.unit_id ?? "").trim()
      if (unitId) units.add(unitId)
    })

    const scopedUnits = [...units]
    params.realtimeScopeStore.setRealtimeUnitScope(params.scopeId, scopedUnits)

    const sortedUnits = scopedUnits.slice().sort((left, right) => left.localeCompare(right))
    const deviceByUnit = new Map(params.deviceStore.devices.map((device: Device) => [device.unit_id, device.id] as const))
    const resolvedDeviceIds: number[] = []
    const readyDeviceIds: number[] = []
    const missingChannelDeviceIds: number[] = []

    for (const unitId of sortedUnits) {
      const deviceId = deviceByUnit.get(unitId)
      if (!Number.isFinite(deviceId as number)) {
        continue
      }
      const numericDeviceId = Number(deviceId)
      resolvedDeviceIds.push(numericDeviceId)
      if (params.channelStore.hasDeviceChannels(numericDeviceId)) {
        readyDeviceIds.push(numericDeviceId)
        continue
      }
      missingChannelDeviceIds.push(numericDeviceId)
    }

    const scopeKey = [
      sortedUnits.join("|"),
      `resolved:${resolvedDeviceIds.slice().sort((a, b) => a - b).join(",")}`,
      `ready:${readyDeviceIds.slice().sort((a, b) => a - b).join(",")}`,
    ].join("::")
    if (scopeKey === lastRealtimeScopeKey) {
      return
    }
    lastRealtimeScopeKey = scopeKey

    for (const deviceId of missingChannelDeviceIds) {
      if (realtimeScopeChannelHydrationInFlight.has(deviceId)) {
        continue
      }
      realtimeScopeChannelHydrationInFlight.add(deviceId)
      void params.channelStore.ensureDeviceChannelsLoaded(deviceId)
        .catch(() => {
          return
        })
        .finally(() => {
          realtimeScopeChannelHydrationInFlight.delete(deviceId)
          scheduleRealtimeUnitScopeSync()
        })
    }

    for (const deviceId of readyDeviceIds) {
      params.channelStore.requestStates(deviceId, { includeDiagnostics: false, silent: true })
    }
  }

  function scheduleRealtimeUnitScopeSync() {
    if (realtimeScopeSyncFrame !== null) {
      return
    }
    realtimeScopeSyncFrame = requestAnimationFrame(() => {
      realtimeScopeSyncFrame = null
      syncRealtimeUnitScope()
    })
  }

  const realtimeScopeSyncTrigger = computed(() => {
    const workspaceId = params.activeWorkspaceId.value ?? 0
    return [
      `ws:${workspaceId}`,
      `alloc:${params.allocationRevision.value}`,
      `channels:${params.channels.value.length}`,
      `busy:${params.isTestRunBusy.value ? 1 : 0}`,
      `devices:${params.devicesRevision.value}`,
    ].join("::")
  })

  watch(
    () => [realtimeScopeSyncTrigger.value, params.isWsConnected.value] as const,
    ([_, connected], previous) => {
      const prevConnected = previous?.[1]

      if (!connected) {
        if (wsScopeRetryTimer !== null) {
          clearTimeout(wsScopeRetryTimer)
          wsScopeRetryTimer = null
        }
        return
      }

      if (!params.isTestRunBusy.value) {
        scheduleRealtimeUnitScopeSync()
      }

      if (prevConnected === true) {
        return
      }

      if (wsScopeRetryTimer !== null) {
        clearTimeout(wsScopeRetryTimer)
      }
      wsScopeRetryTimer = setTimeout(() => {
        wsScopeRetryTimer = null
        scheduleRealtimeUnitScopeSync()
      }, 700)
    },
    { immediate: true, flush: "post" },
  )

  onBeforeUnmount(() => {
    if (wsScopeRetryTimer !== null) {
      clearTimeout(wsScopeRetryTimer)
      wsScopeRetryTimer = null
    }
    if (realtimeScopeSyncFrame !== null) {
      cancelAnimationFrame(realtimeScopeSyncFrame)
      realtimeScopeSyncFrame = null
    }
    params.realtimeScopeStore.clearRealtimeUnitScope(params.scopeId)
  })

  return {
    syncRealtimeUnitScope,
    scheduleRealtimeUnitScopeSync,
  }
}
