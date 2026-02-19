<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col gap-4 p-3 md:p-4">
    <AllocationEditorHeader
      :summary-text="summaryText"
      :workspace-missing="workspaceMissing"
      :loading="loading"
      :allocated-cable-rows-count="allocatedCableRows.length"
      :allocation-rows-count="allocationRows.length"
      :updating-allocations="updatingAllocations"
      :allocating-selected="allocatingSelected"
      :deallocating-selected="deallocatingSelected"
      :allocation-jobs-running="allocationJobsRunning"
      :allocation-job-progress-text="allocationJobProgressText"
      :show-test-run-progress="showTestRunProgress"
      :test-run-progress-text="testRunProgressText"
      :test-run-progress-percent="testRunProgressPercent"
      :can-pause-active-test-run="canPauseActiveTestRun"
      :can-resume-active-test-run="canResumeActiveTestRun"
      :can-stop-active-test-run="canStopActiveTestRun"
      :test-run-control-busy="testRunControlBusy"
      :is-stopping-active-test-run="isStoppingActiveTestRun"
      :last-test-summary-text="lastTestSummaryText"
      :can-allocate-selected="selectedAllocatableUnassignedSignalIds.length > 0"
      :can-deallocate-selected="selectedAllocatedSignalIds.length > 0"
      :can-run-test="selectedAllocatedPhysicalRows.length > 0 || isTestRunBusy"
      :is-test-run-busy="isTestRunBusy"
      :test-run-toggle-mode="testRunToggleMode"
      :test-run-interval-ms="testRunIntervalMs"
      :can-create-switchgear-from-selection="canCreateSwitchgearFromSelection"
      :switchgear-create-in-progress="switchgearCreateInProgress"
      :create-switchgear-button-label="createSwitchgearButtonLabel"
      @import="openImportModal"
      @export-cable="openExportModal"
      @export-report="exportSignalReport"
      @allocate-selected="allocateSelectedUnassigned"
      @deallocate-selected="deallocateSelected"
      @run-test="runTestVisualOnly"
      @set-toggle-mode="setTestRunToggleMode"
      @set-interval-ms="setTestRunIntervalMs"
      @control-test-run="controlActiveTestRun"
      @dismiss-last-test="dismissLastTestSummary"
      @create-switchgear="createSwitchgearVisualOnly"
    />

    <div
      v-if="workspaceMissing"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 bg-white/80 p-8 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/40 dark:text-neutral-400"
    >
      Select a workspace to manage signal allocations.
    </div>

    <div
      v-else-if="showInitialPageLoading"
      class="flex flex-1 flex-col gap-3 rounded-2xl border border-neutral-200 bg-white/90 p-4 dark:border-neutral-800 dark:bg-neutral-900/80"
      aria-live="polite"
      aria-busy="true"
    >
      <div class="flex items-center gap-2 text-sm font-medium text-neutral-600 dark:text-neutral-300">
        <span class="h-2.5 w-2.5 animate-pulse rounded-full bg-emerald-500"></span>
        <span>Loading signal sheet…</span>
      </div>
      <div class="space-y-2">
        <div class="h-8 animate-pulse rounded-lg bg-neutral-100 dark:bg-neutral-800"></div>
        <div class="h-8 animate-pulse rounded-lg bg-neutral-100 dark:bg-neutral-800"></div>
        <div class="h-8 animate-pulse rounded-lg bg-neutral-100 dark:bg-neutral-800"></div>
        <div class="h-8 animate-pulse rounded-lg bg-neutral-100 dark:bg-neutral-800"></div>
      </div>
    </div>

    <div
      v-else-if="!activeSignalSheet || activeSignalSheet.signals_count === 0"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 bg-white/80 p-8 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/40 dark:text-neutral-400"
    >
      Import a signal list to start allocating channels.
    </div>

    <UiAffinoDataGrid
      v-else
      class="flex-1 min-h-0"
      :rows="gridRows"
      :columns="gridColumns"
      :row-height="34"
      :overscan-rows="10"
      :overscan-columns="2"
      :enable-filtering="true"
      :enable-column-resize="true"
      :empty-text="'No signals available.'"
      :row-key="rowKey"
      :show-controls="true"
      :table-id="gridTableId"
      :persist-state="true"
      :dataset-key="gridDatasetKey"
      :selected-row-keys="selectedRowKeys"
      @row-click="handleRowClick"
      @selection-change="handleSelectionChange"
    >
      <template #cell="{ column, row, value }">
        <div v-if="column.key === 'channel_select'" class="flex h-full w-full items-center">
          <AllocationChannelPicker
            :row="asAllocationRow(row)"
            :loading="loading"
            :value-label="allocationDisplayLabel(asAllocationRow(row))"
            :value-class="allocationValueClass(asAllocationRow(row))"
            :channel-groups-resolver="channelGroupsResolverForRow(asAllocationRow(row))"
            :unit-status-by-id="unitStatusById"
            @allocate="channelId => handleAllocationPickerSelect(asAllocationRow(row), channelId)"
          />
        </div>

        <div v-else-if="column.key === 'control'" class="flex h-full items-center">
          <AllocationControlCell
            :can-control="canControl(asAllocationRow(row))"
            :lamp-class="controlLampClass(asAllocationRow(row))"
            :status-class="controlStatusClass(asAllocationRow(row))"
            :status-tag="controlStatusTag(asAllocationRow(row))"
            :state-label="controlStateLabel(asAllocationRow(row))"
            :disabled="controlDisabled(asAllocationRow(row))"
            @set-on="() => handleControlMenuSelect(asAllocationRow(row), true)"
            @set-off="() => handleControlMenuSelect(asAllocationRow(row), false)"
          />
        </div>

        <span
          v-else-if="column.key === 'last_tested_at'"
          class="text-xs text-neutral-700 dark:text-neutral-100"
        >
          {{ formatTestedAt(resolveLastTestedAtValue(asAllocationRow(row), value)) }}
        </span>

        <span v-else class="text-xs text-neutral-700 dark:text-neutral-100">{{ formatCell(value) }}</span>
      </template>
    </UiAffinoDataGrid>

    <SignalImportModal :open="importModalOpen" @close="closeImportModal" @imported="handleImported" />
    <SignalExportModal
      :open="exportModalOpen"
      :workspace-id="workspaceStore.activeWorkspaceId"
      :required-columns="requiredExportColumnOptions"
      :optional-columns="optionalExportColumnOptions"
      @close="closeExportModal"
      @export="handleExportCableFromWizard"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, ref, shallowRef, triggerRef, watch } from "vue"
import { storeToRefs } from "pinia"
import { useRoute, useRouter } from "vue-router"

import UiAffinoDataGrid from "@/components/ui/UiAffinoDataGrid.vue"
import { SignalSheetAPI } from "@/api/signal_sheet.api"
import type { Channel, DoChannel } from "@/types/channel"
import type { SignalAllocationJob, SignalAllocationRow } from "@/types/signal"
import AllocationControlCell from "@/pages/signals/components/AllocationControlCell.vue"
import AllocationEditorHeader from "@/pages/signals/components/AllocationEditorHeader.vue"
import AllocationChannelPicker from "@/pages/signals/components/AllocationChannelPicker.vue"
import SignalExportModal, { type ExportColumnOption } from "@/pages/signals/components/SignalExportModal.vue"
import SignalImportModal from "@/pages/signals/components/SignalImportModal.vue"
import { extractSourceRowFromSignalMetadata, resolveAllSourceColumnHeaders } from "@/pages/signals/utils/sourceColumns"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"
import { useSignalJobStore } from "@/stores/signalJobStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { resolveRuntimeChannelTypeForSignal } from "@/utils/signalRuntimeMapping"

const signalSheetStore = useSignalSheetStore()
const workspaceStore = useWorkspaceStore()
const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
const realtimeScopeStore = useRealtimeScopeStore()
const signalJobStore = useSignalJobStore()
const switchgearStore = useSwitchgearStore()
const toastStore = useToastStore()
const route = useRoute()
const router = useRouter()

const { allocationRows, loadingAllocations, loadingSheet, updatingAllocations, allocatedCount, allocationRevision, recentlyChangedSignalIds } = storeToRefs(signalSheetStore)
const { activeJobs, jobsById } = storeToRefs(signalJobStore)
const { channels } = storeToRefs(channelStore)

const scopeId = "signals:allocations"
const importModalOpen = ref(false)
const exportModalOpen = ref(false)
const selectedRowKeys = ref<string[]>([])
const allocatingSelected = ref(false)
const deallocatingSelected = ref(false)
const testRunInProgress = ref(false)
const testRunIntervalMs = ref(1000)
const testRunToggleMode = ref<"single" | "double">("single")
const switchgearCreateInProgress = ref(false)
const testRunTotal = ref(0)
const testRunProcessed = ref(0)
const testRunSucceeded = ref(0)
const testRunSkipped = ref(0)
const testRunControlBusy = ref(false)
const testRunStartedAtMs = ref<number | null>(null)
const testedAtOverlayBySignalId = shallowRef<Map<number, string>>(new Map())
const pendingTestedAtOverlayBySignalId = new Map<number, string>()
let testedAtOverlayFlushTimer: ReturnType<typeof setTimeout> | null = null
const TESTED_AT_OVERLAY_FLUSH_MS = 120
const MAX_RESTORED_SELECTION_KEYS = 2000

type LastTestSummary = {
  jobId: string
  status: SignalAllocationJob["status"]
  finishedAtIso: string
  processed: number
  succeeded: number
  skipped: number
  durationMs: number
}

const lastTestSummary = ref<LastTestSummary | null>(null)
const dismissedLastTestJobId = ref<string | null>(null)
let realtimeScopeSyncFrame: number | null = null
let lastRealtimeScopeKey = ""

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const loading = computed(() => loadingAllocations.value || loadingSheet.value || updatingAllocations.value)
const showInitialPageLoading = computed(() => (
  !workspaceMissing.value
  && loading.value
  && !signalSheetStore.sheet
  && allocationRows.value.length === 0
))
const activeSignalSheet = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId
  const sheet = signalSheetStore.sheet
  if (!workspaceId || !sheet) {
    return null
  }
  if (sheet.workspace_id !== workspaceId) {
    return null
  }
  return sheet
})

const testedSignalsCount = computed(() => (
  allocationRows.value.reduce((count, row) => (row.tested_at ? count + 1 : count), 0)
))

const totalSignalsCount = computed(() => {
  const sheet = activeSignalSheet.value
  if (!sheet) return allocationRows.value.length
  return Math.max(0, Number(sheet.signals_count ?? 0))
})

const remainingSignalsCount = computed(() => (
  Math.max(0, totalSignalsCount.value - testedSignalsCount.value)
))

const summaryText = computed(() => {
  const sheet = activeSignalSheet.value
  if (!sheet) return "No active sheet"
  const total = totalSignalsCount.value
  const allocated = Math.max(0, allocatedCount.value)
  const tested = testedSignalsCount.value
  const remaining = remainingSignalsCount.value
  return [
    `${total} signals`,
    `${allocated} allocated (${formatPercentCompact(allocated, total)})`,
    `${tested} tested (${formatPercentCompact(tested, total)})`,
    `${remaining} remaining (${formatPercentCompact(remaining, total)})`,
  ].join(" · ")
})

const gridDatasetKey = computed(() => {
  const sheet = activeSignalSheet.value
  if (!sheet) return ""
  const selectedColumns = Array.isArray(sheet.import_meta?.selected_columns)
    ? sheet.import_meta?.selected_columns.map(item => String(item)).join("|")
    : ""
  return [
    sheet.source_hash ?? "",
    String(sheet.schema_version),
    String(sheet.rows_count),
    String(sheet.signals_count),
    selectedColumns,
  ].join("::")
})

const selectionStorageKey = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId ?? "none"
  const sheetId = activeSignalSheet.value ? String(activeSignalSheet.value.id) : "none"
  const datasetKey = gridDatasetKey.value || "dataset"
  return `signals:allocations:selected:${workspaceId}:${sheetId}:${datasetKey}`
})

const gridTableId = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId ?? "none"
  const sheetId = activeSignalSheet.value ? String(activeSignalSheet.value.id) : "none"
  return `signals-allocation-grid-${workspaceId}-${sheetId}`
})

function restoreSelectedRowKeysFromStorage() {
  if (typeof window === "undefined") {
    return
  }
  try {
    const raw = window.localStorage.getItem(selectionStorageKey.value)
    if (!raw) {
      selectedRowKeys.value = []
      return
    }
    const parsed = JSON.parse(raw)
    if (!Array.isArray(parsed)) {
      selectedRowKeys.value = []
      return
    }
    selectedRowKeys.value = parsed
      .map(item => String(item).trim())
      .filter(item => /^signal-\d+$/.test(item))
      .filter(item => item.length > 0)
      .slice(0, MAX_RESTORED_SELECTION_KEYS)
  } catch {
    selectedRowKeys.value = []
  }
}

function persistSelectedRowKeysToStorage() {
  if (typeof window === "undefined") {
    return
  }
  try {
    if (selectedRowKeys.value.length === 0) {
      window.localStorage.removeItem(selectionStorageKey.value)
      return
    }
    window.localStorage.setItem(selectionStorageKey.value, JSON.stringify(selectedRowKeys.value))
  } catch {
    // Ignore storage write failures and keep runtime functional.
  }
}

const channelMap = computed(() => {
  const map = new Map<number, Channel>()
  channels.value.forEach(channel => map.set(channel.id, channel))
  return map
})

const deviceStatusById = computed(() => {
  const map = new Map<number, string>()
  deviceStore.devices.forEach((device) => {
    map.set(device.id, device.status)
  })
  return map
})

const unitStatusById = computed<Record<string, string>>(() => {
  const map: Record<string, string> = {}
  deviceStore.devices.forEach((device) => {
    const unitId = String(device.unit_id ?? "").trim()
    if (!unitId) return
    map[unitId] = device.status
  })
  return map
})

const channelUnitById = computed(() => {
  const map = new Map<number, string>()
  channels.value.forEach((channel) => {
    const unitId = channelStore.resolveUnitId(channel.device_id)
    map.set(channel.id, unitId || `Device ${channel.device_id}`)
  })
  return map
})

const allocationRowBySignalId = computed(() => {
  const map = new Map<number, SignalAllocationRow>()
  allocationRows.value.forEach((row) => {
    map.set(row.signal_id, row)
  })
  return map
})

const freeChannelCountByType = computed(() => {
  const usedChannelIds = new Set<number>()
  allocationRows.value.forEach((row) => {
    if (Number.isFinite(row.channel_id as number)) {
      usedChannelIds.add(Number(row.channel_id))
    }
  })

  const counts: Record<"di" | "do" | "ai" | "ao", number> = {
    di: 0,
    do: 0,
    ai: 0,
    ao: 0,
  }

  channels.value.forEach((channel) => {
    if (usedChannelIds.has(channel.id)) {
      return
    }
    const type = normalizedChannelType(channel.type)
    if (!type) {
      return
    }
    counts[type] += 1
  })

  return counts
})

function signalIdFromRowKey(rowKey: string): number | null {
  if (!rowKey.startsWith("signal-")) return null
  const parsed = Number(rowKey.slice("signal-".length))
  if (!Number.isFinite(parsed)) return null
  return parsed
}

const selectedAllocationRows = computed(() => (
  selectedRowKeys.value
    .map((rowKey) => {
      const signalId = signalIdFromRowKey(rowKey)
      if (signalId === null) return null
      return allocationRowBySignalId.value.get(signalId) ?? null
    })
    .filter((row): row is SignalAllocationRow => Boolean(row))
))

const selectedUnassignedSignalIds = computed(() => (
  selectedAllocationRows.value
    .filter(row => !Number.isFinite(row.channel_id as number))
    .map(row => row.signal_id)
))

const selectedAllocatableUnassignedSignalIds = computed(() => (
  selectedAllocationRows.value
    .filter((row) => !Number.isFinite(row.channel_id as number))
    .filter((row) => {
      const required = resolveRuntimeChannelTypeForSignal(row.signal_direction)
      if (!required) {
        return false
      }
      return freeChannelCountByType.value[required] > 0
    })
    .map(row => row.signal_id)
))

const selectedAllocatedSignalIds = computed(() => (
  selectedAllocationRows.value
    .filter(row => Number.isFinite(row.channel_id as number))
    .map(row => row.signal_id)
))

const selectedSwitchgearRows = computed(() => (
  selectedAllocationRows.value.filter(row => row.signal_direction === "DI" || row.signal_direction === "DO")
))

const selectedSwitchgearOnlyDiDo = computed(() => (
  selectedAllocationRows.value.length > 0 && selectedSwitchgearRows.value.length === selectedAllocationRows.value.length
))

const selectedSwitchgearDiCount = computed(() => (
  selectedSwitchgearRows.value.filter(row => row.signal_direction === "DI").length
))

const selectedSwitchgearDoCount = computed(() => (
  selectedSwitchgearRows.value.filter(row => row.signal_direction === "DO").length
))

const selectedAllocatedPhysicalRows = computed(() => (
  selectedAllocationRows.value.filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.device_id as number)
    && Boolean(row.unit_id)
  ))
))

const switchgearCreatableCount = computed(() => {
  if (!selectedSwitchgearOnlyDiDo.value) return 0
  const diCount = selectedSwitchgearDiCount.value
  const doCount = selectedSwitchgearDoCount.value
  if (diCount < 2 || doCount < 2) return 0
  if (diCount !== doCount) return 0
  if (diCount % 2 !== 0) return 0
  return diCount / 2
})

const canCreateSwitchgearFromSelection = computed(() => switchgearCreatableCount.value > 0)

const createSwitchgearButtonLabel = computed(() => (
  switchgearCreatableCount.value === 1
    ? "Create switchgear"
    : `Create ${switchgearCreatableCount.value} switchgears`
))

const allocatedCableRows = computed(() => (
  allocationRows.value.filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.channel_index as number)
    && Boolean(String(row.unit_id ?? "").trim())
  ))
))

const activeAllocationJob = computed(() => (
  activeJobs.value.find(job => String(job.operation) !== "test_run") ?? null
))

const allocationJobsRunning = computed(() => Boolean(activeAllocationJob.value))

const allocationJobProgressText = computed(() => {
  const job = activeAllocationJob.value
  if (!job) return null
  const total = Math.max(0, Number(job.progress_total ?? 0))
  const done = Math.max(0, Number(job.progress_done ?? 0))
  const stateLabel = job.status === "queued"
    ? "Queued"
    : job.status === "paused"
      ? "Paused"
      : job.status === "cancelling"
        ? "Cancelling"
        : "Running"
  const message = String(job.message ?? "").trim()
  if (total <= 0) {
    return message || stateLabel
  }
  const safeDone = Math.min(done, total)
  const percent = Math.max(0, Math.min(100, Math.round((safeDone / total) * 100)))
  const detail = `${percent}% · ${safeDone}/${total}`
  if (message && message.toLowerCase() !== stateLabel.toLowerCase()) {
    return `${message} · ${detail}`
  }
  return `${stateLabel} ${detail}`
})

const showTestRunProgress = computed(() => {
  const activeTestJob = activeJobs.value.find(job => String(job.operation) === "test_run")
  if (activeTestJob) {
    return Number(activeTestJob.progress_total ?? 0) > 0
  }
  return testRunInProgress.value && testRunTotal.value > 0
})

const testRunProgressPercent = computed(() => {
  const total = testRunTotal.value
  if (!total) return 0
  return Math.max(0, Math.min(100, Math.round((testRunProcessed.value / total) * 100)))
})

const testRunEtaSeconds = computed(() => {
  if (!isTestRunBusy.value) return 0
  const remaining = Math.max(0, testRunTotal.value - testRunProcessed.value)
  const perSignalFactor = testRunToggleMode.value === "double" ? 2 : 1
  return remaining * ((testRunIntervalMs.value * perSignalFactor) / 1000)
})

const testRunProgressText = computed(() => {
  const base = `${testRunProcessed.value}/${testRunTotal.value} · ok ${testRunSucceeded.value} · skip ${testRunSkipped.value}`
  if (!isTestRunBusy.value) return base
  return `${base} · ETA ${formatDurationShort(testRunEtaSeconds.value)}`
})

const activeTestRunJob = computed(() => (
  activeJobs.value.find(job => String(job.operation) === "test_run") ?? null
))
const isTestRunBusy = computed(() => {
  const status = String(activeTestRunJob.value?.status ?? "")
  const activeJobBusy = status === "queued" || status === "running" || status === "cancelling"
  return testRunInProgress.value || activeJobBusy
})
const latestCompletedTestRunJob = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) {
    return null
  }

  return Object.values(jobsById.value)
    .filter(job => job.workspace_id === workspaceId)
    .filter(job => String(job.operation) === "test_run")
    .filter(job => ["succeeded", "failed", "cancelled"].includes(String(job.status)))
    .sort((left, right) => String(right.updated_at).localeCompare(String(left.updated_at)))[0] ?? null
})

const canPauseActiveTestRun = computed(() => activeTestRunJob.value?.status === "running")
const canResumeActiveTestRun = computed(() => activeTestRunJob.value?.status === "paused")
const canStopActiveTestRun = computed(() => {
  const status = String(activeTestRunJob.value?.status ?? "")
  return status === "queued" || status === "running" || status === "paused"
})
const isStoppingActiveTestRun = computed(() => activeTestRunJob.value?.status === "cancelling")

watch(activeTestRunJob, (job) => {
  if (!job) {
    testRunInProgress.value = false
    flushTestedAtOverlayBatch()
    clearTestedAtOverlay()
    scheduleRealtimeUnitScopeSync()
    return
  }

  testRunInProgress.value = String(job.status ?? "") !== "paused"
  updateTestRunStatsFromJob(job)
}, { immediate: true })

watch(latestCompletedTestRunJob, (job) => {
  if (!job) {
    return
  }
  if (dismissedLastTestJobId.value && dismissedLastTestJobId.value === job.job_id) {
    return
  }
  setLastTestSummaryFromJob(job)
}, { immediate: true })

const sourceColumnHeaders = computed(() => (
  resolveAllSourceColumnHeaders(activeSignalSheet.value, allocationRows.value)
))

const gridColumns = computed(() => {
  const sourceColumns = sourceColumnHeaders.value.map((header, index) => ({
    key: sourceColumnKey(index),
    label: header,
    width: Math.min(Math.max(header.length * 11, 140), 360),
    minWidth: 120,
  }))

  return [
    ...sourceColumns,
    { key: "channel_select", label: "Unit/Channel", width: 150, minWidth: 120, pin: "right" as const },
    { key: "last_tested_at", label: "Last tested", width: 180, minWidth: 150, pin: "right" as const },
    { key: "control", label: "Control", width: 120, minWidth: 96, pin: "right" as const, meta: { filterable: false, sortable: false } },
  ]
})

const gridRows = shallowRef<GridRow[]>([])
const gridRowBySignalId = new Map<number, GridRow>()
const gridRowIndexBySignalId = new Map<number, number>()
const channelGroupsResolverBySignalId = new Map<number, () => ChannelOptionGroup[]>()

type GridRow = Record<string, unknown>
type ChannelOption = { id: number; label: string; disabled: boolean }
type ChannelOptionGroup = { unitId: string; options: ChannelOption[] }

function sourceColumnKey(index: number): string {
  return `source_col_${index}`
}

function assignDynamicGridFields(payload: GridRow, row: SignalAllocationRow) {
  Object.assign(payload, row)
  payload.rowId = `signal-${row.signal_id}`
  // Filtering/sorting must use the same human-readable text as in the cell.
  payload.channel_select = allocationDisplayLabel(row)
  payload.last_tested_at = row.tested_at
  payload.control = ""
}

function flushTestedAtOverlayBatch() {
  if (testedAtOverlayFlushTimer !== null) {
    clearTimeout(testedAtOverlayFlushTimer)
    testedAtOverlayFlushTimer = null
  }
  if (pendingTestedAtOverlayBySignalId.size === 0) {
    return
  }

  const overlay = testedAtOverlayBySignalId.value
  pendingTestedAtOverlayBySignalId.forEach((testedAtIso, signalId) => {
    overlay.set(signalId, testedAtIso)
  })
  pendingTestedAtOverlayBySignalId.clear()
  triggerRef(testedAtOverlayBySignalId)
}

function queueTestedAtOverlay(signalIds: readonly number[]) {
  if (!signalIds.length) {
    return
  }

  signalIds.forEach((signalId) => {
    const row = findAllocationRowBySignalId(signalId)
    if (!row) return
    const testedAtIso = String(row.tested_at ?? "").trim()
    if (!testedAtIso) return
    pendingTestedAtOverlayBySignalId.set(signalId, testedAtIso)
  })

  if (pendingTestedAtOverlayBySignalId.size === 0 || testedAtOverlayFlushTimer !== null) {
    return
  }

  testedAtOverlayFlushTimer = setTimeout(() => {
    flushTestedAtOverlayBatch()
  }, TESTED_AT_OVERLAY_FLUSH_MS)
}

function clearTestedAtOverlay() {
  pendingTestedAtOverlayBySignalId.clear()
  if (testedAtOverlayFlushTimer !== null) {
    clearTimeout(testedAtOverlayFlushTimer)
    testedAtOverlayFlushTimer = null
  }
  if (testedAtOverlayBySignalId.value.size === 0) {
    return
  }
  testedAtOverlayBySignalId.value.clear()
  triggerRef(testedAtOverlayBySignalId)
}

function createGridRow(row: SignalAllocationRow, headers: readonly string[]): GridRow {
  const payload: GridRow = {}
  assignDynamicGridFields(payload, row)
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  headers.forEach((header, index) => {
    payload[sourceColumnKey(index)] = sourceRow[header] ?? ""
  })
  return payload
}

function rebuildGridRows() {
  const headers = sourceColumnHeaders.value
  const nextRows: GridRow[] = []
  const activeSignalIds = new Set<number>()
  gridRowBySignalId.clear()
  gridRowIndexBySignalId.clear()

  allocationRows.value.forEach((row, index) => {
    const signalId = row.signal_id
    activeSignalIds.add(signalId)
    const payload = createGridRow(row, headers)
    gridRowBySignalId.set(signalId, payload)
    gridRowIndexBySignalId.set(signalId, index)
    nextRows.push(payload)
  })

  channelGroupsResolverBySignalId.forEach((_, signalId) => {
    if (!activeSignalIds.has(signalId)) {
      channelGroupsResolverBySignalId.delete(signalId)
    }
  })

  if (selectedRowKeys.value.length > 0) {
    const allowed = new Set(nextRows.map(item => String(item.rowId)))
    selectedRowKeys.value = selectedRowKeys.value.filter(rowKey => allowed.has(rowKey))
  }

  gridRows.value = nextRows
}

function findAllocationRowBySignalId(signalId: number): SignalAllocationRow | null {
  return allocationRowBySignalId.value.get(signalId) ?? null
}

function syncGridRowsBySignalIds(signalIds: readonly number[]) {
  if (!signalIds.length) return
  if (signalIds.length > 128) {
    rebuildGridRows()
    return
  }

  const headers = sourceColumnHeaders.value
  let structuralChange = false

  signalIds.forEach((signalId) => {
    const row = findAllocationRowBySignalId(signalId)
    if (!row) {
      const existingIndex = gridRowIndexBySignalId.get(signalId)
      if (existingIndex === undefined) return
      const nextRows = [...gridRows.value]
      nextRows.splice(existingIndex, 1)
      gridRows.value = nextRows
      structuralChange = true
      return
    }

    const existingPayload = gridRowBySignalId.get(signalId)
    if (!existingPayload) {
      structuralChange = true
      return
    }
    assignDynamicGridFields(existingPayload, row)
  })

  if (structuralChange) {
    rebuildGridRows()
    return
  }

  // shallowRef: notify grid about in-place patched row objects.
  triggerRef(gridRows)
}

function asAllocationRow(row: GridRow): SignalAllocationRow {
  return row as unknown as SignalAllocationRow
}

function rowKey(row: Record<string, unknown>) {
  return String(row.rowId)
}

function resolveLastTestedAtValue(row: SignalAllocationRow, fallback: unknown): unknown {
  const signalId = Number(row.signal_id)
  if (Number.isFinite(signalId)) {
    const overlay = testedAtOverlayBySignalId.value.get(signalId)
    if (overlay) {
      return overlay
    }
  }
  return fallback
}

function formatCell(value: unknown) {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  try {
    return JSON.stringify(value)
  } catch {
    return String(value)
  }
}

function formatTestedAt(value: unknown): string {
  const raw = String(value ?? "").trim()
  if (!raw) return "—"
  const parsed = new Date(raw)
  if (Number.isNaN(parsed.getTime())) {
    return raw
  }
  const formatted = new Intl.DateTimeFormat(undefined, {
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  }).format(parsed)
  const milliseconds = String(parsed.getMilliseconds()).padStart(3, "0")
  return `${formatted}.${milliseconds}`
}

function formatPercentCompact(part: number, total: number): string {
  if (!Number.isFinite(total) || total <= 0) return "0%"
  const value = Math.max(0, (part / total) * 100)
  const rounded = Math.round(value * 10) / 10
  return Number.isInteger(rounded) ? `${rounded.toFixed(0)}%` : `${rounded.toFixed(1)}%`
}

function toFilenamePart(value: string | null | undefined): string {
  const normalized = String(value ?? "")
    .trim()
    .replace(/\s+/g, "-")
    .replace(/[^a-zA-Z0-9._-]/g, "-")
    .replace(/-+/g, "-")
    .replace(/^-+|-+$/g, "")
  return normalized || "workspace"
}

function formatDurationShort(seconds: number): string {
  const normalized = Math.max(0, Math.round(seconds))
  const minutes = Math.floor(normalized / 60)
  const remSeconds = normalized % 60
  if (minutes <= 0) {
    return `${remSeconds}s`
  }
  return `${minutes}m ${remSeconds}s`
}

function formatDateTimeShort(iso: string): string {
  const parsed = new Date(iso)
  if (Number.isNaN(parsed.getTime())) {
    return iso
  }
  const formatted = new Intl.DateTimeFormat(undefined, {
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  }).format(parsed)
  return formatted
}

function formatLastTestSummary(summary: LastTestSummary): string {
  const statusPrefix = summary.status === "failed"
    ? "failed"
    : summary.status === "cancelled"
      ? "cancelled"
      : "completed"

  return [
    statusPrefix,
    `${formatDateTimeShort(summary.finishedAtIso)}`,
    `${summary.succeeded}/${summary.processed} toggled`,
    summary.skipped > 0 ? `${summary.skipped} skipped` : "",
    `${formatDurationShort(summary.durationMs / 1000)}`,
  ].filter(Boolean).join(" · ")
}

function dismissLastTestSummary() {
  dismissedLastTestJobId.value = lastTestSummary.value?.jobId ?? null
  lastTestSummary.value = null
}

function setLastTestSummary(job: SignalAllocationJob, startedAtMs: number | null) {
  dismissedLastTestJobId.value = null
  const processed = Math.max(0, readNumericResult(job, "processed") || Number(job.progress_done ?? 0) || testRunProcessed.value)
  const succeeded = Math.max(0, readNumericResult(job, "succeeded") || testRunSucceeded.value)
  const skipped = Math.max(0, readNumericResult(job, "skipped") || testRunSkipped.value)
  const finishedAtMs = Date.now()
  const durationMs = Math.max(0, startedAtMs ? finishedAtMs - startedAtMs : 0)
  lastTestSummary.value = {
    jobId: String(job.job_id),
    status: job.status,
    finishedAtIso: new Date(finishedAtMs).toISOString(),
    processed,
    succeeded,
    skipped,
    durationMs,
  }
}

function setLastTestSummaryFromJob(job: SignalAllocationJob) {
  dismissedLastTestJobId.value = null
  const processed = Math.max(0, readNumericResult(job, "processed") || Number(job.progress_done ?? 0))
  const succeeded = Math.max(0, readNumericResult(job, "succeeded"))
  const skipped = Math.max(0, readNumericResult(job, "skipped"))

  const finishedAtParsed = Date.parse(String(job.updated_at ?? ""))
  const createdAtParsed = Date.parse(String(job.created_at ?? ""))
  const finishedAtMs = Number.isFinite(finishedAtParsed) ? finishedAtParsed : Date.now()
  const durationMs = (Number.isFinite(createdAtParsed) && Number.isFinite(finishedAtParsed))
    ? Math.max(0, finishedAtParsed - createdAtParsed)
    : 0

  lastTestSummary.value = {
    jobId: String(job.job_id),
    status: job.status,
    finishedAtIso: new Date(finishedAtMs).toISOString(),
    processed,
    succeeded,
    skipped,
    durationMs,
  }
}

const lastTestSummaryText = computed(() => (
  lastTestSummary.value ? formatLastTestSummary(lastTestSummary.value) : ""
))

function csvEscape(value: unknown): string {
  const text = String(value ?? "")
  if (/[",\n\r]/.test(text)) {
    return `"${text.replace(/"/g, "\"\"")}"`
  }
  return text
}

function downloadTextFile(content: string, filename: string, mimeType = "text/csv;charset=utf-8;") {
  const blob = new Blob([`\uFEFF${content}`], { type: mimeType })
  const url = URL.createObjectURL(blob)
  const link = document.createElement("a")
  link.href = url
  link.download = filename
  document.body.appendChild(link)
  link.click()
  link.remove()
  URL.revokeObjectURL(url)
}

function resolveTerminalHeader(): string | null {
  const fromImportMeta = String(activeSignalSheet.value?.import_meta?.terminal_column ?? "").trim()
  if (fromImportMeta) {
    return fromImportMeta
  }

  const headers = sourceColumnHeaders.value
  const byHeuristic = headers.find((header) => /terminal|клем|клемм|xt/i.test(String(header)))
  if (byHeuristic) {
    return String(byHeuristic)
  }

  return null
}

function resolveTerminalValue(row: SignalAllocationRow, terminalHeader: string | null): string {
  if (!terminalHeader) return ""
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const rawValue = sourceRow[terminalHeader]
  if (rawValue === undefined || rawValue === null) return ""
  return String(rawValue)
}

type CableExportColumnDef = {
  key: string
  label: string
  required: boolean
  getValue: (row: SignalAllocationRow, terminalHeader: string | null) => string
}

const cableExportColumnDefs = computed<CableExportColumnDef[]>(() => {
  const sourceColumnDefs: CableExportColumnDef[] = sourceColumnHeaders.value.map((header) => ({
    key: `source:${header}`,
    label: header,
    required: false,
    getValue: (row: SignalAllocationRow) => {
      const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
      const rawValue = sourceRow[header]
      return rawValue === undefined || rawValue === null ? "" : String(rawValue)
    },
  }))

  return [
    {
      key: "unit_id",
      label: "unit_id",
      required: true,
      getValue: (row: SignalAllocationRow) => String(row.unit_id ?? ""),
    },
    {
      key: "channel_id",
      label: "channel_id",
      required: true,
      getValue: (row: SignalAllocationRow) => String(row.channel_id ?? ""),
    },
    {
      key: "terminal",
      label: "terminal",
      required: true,
      getValue: (row: SignalAllocationRow, terminalHeader: string | null) => resolveTerminalValue(row, terminalHeader),
    },
    {
      key: "channel_index",
      label: "channel_index",
      required: false,
      getValue: (row: SignalAllocationRow) => (
        Number.isFinite(row.channel_index as number) ? String(Number(row.channel_index) + 1) : ""
      ),
    },
    {
      key: "channel_label",
      label: "channel_label",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.channel_label ?? ""),
    },
    {
      key: "signal_name",
      label: "signal_name",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_name ?? ""),
    },
    {
      key: "signal_key",
      label: "signal_key",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_key ?? ""),
    },
    {
      key: "signal_direction",
      label: "signal_direction",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_direction ?? ""),
    },
    {
      key: "signal_category",
      label: "signal_category",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.signal_category ?? ""),
    },
    {
      key: "last_tested_at",
      label: "last_tested_at",
      required: false,
      getValue: (row: SignalAllocationRow) => String(row.tested_at ?? ""),
    },
    ...sourceColumnDefs,
  ]
})

const requiredExportColumnOptions = computed<ExportColumnOption[]>(() => (
  cableExportColumnDefs.value
    .filter(column => column.required)
    .map(column => ({ key: column.key, label: column.label }))
))

const optionalExportColumnOptions = computed<ExportColumnOption[]>(() => (
  cableExportColumnDefs.value
    .filter(column => !column.required)
    .map(column => ({ key: column.key, label: column.label }))
))

function resolveSelectedCableExportColumns(optionalColumnKeys: readonly string[]): CableExportColumnDef[] {
  const byKey = new Map(cableExportColumnDefs.value.map(column => [column.key, column] as const))
  const requiredStart = ["unit_id", "channel_id"]
    .map(key => byKey.get(key))
    .filter((column): column is CableExportColumnDef => Boolean(column))
  const terminalColumn = byKey.get("terminal")

  const optional = optionalColumnKeys
    .map(key => byKey.get(key))
    .filter((column): column is CableExportColumnDef => Boolean(column && !column.required))

  return terminalColumn
    ? [...requiredStart, ...optional, terminalColumn]
    : [...requiredStart, ...optional]
}

function buildSignalReportRows(): string[][] {
  const headers = sourceColumnHeaders.value
  const fallbackHeaders = headers.length > 0 ? headers : ["signal_name", "signal_key"]

  return allocationRows.value.map((row) => {
    const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
    const sourceCells = fallbackHeaders.map((header) => {
      if (header === "signal_name") return row.signal_name
      if (header === "signal_key") return row.signal_key
      return sourceRow[header] ?? ""
    })
    const channelNumber = Number.isFinite(row.channel_index as number) ? Number(row.channel_index) + 1 : ""
    return [
      ...sourceCells.map(item => String(item ?? "")),
      String(row.signal_direction ?? ""),
      String(row.unit_id ?? ""),
      String(channelNumber),
      String(row.tested_at ?? ""),
    ]
  })
}

function exportCableJournal(optionalColumnKeys: string[] = []) {
  if (!allocatedCableRows.value.length) {
    toastStore.info("No allocated rows to export.")
    return
  }

  const workspaceName = String(workspaceStore.activeWorkspace?.name ?? "")
  const workspaceFilePart = toFilenamePart(workspaceName)
  const generatedAt = new Date()
  const terminalHeader = resolveTerminalHeader()
  const selectedColumns = resolveSelectedCableExportColumns(optionalColumnKeys)
  const tableHeaders = selectedColumns.map(column => column.label)
  const rows = allocatedCableRows.value.map(row => (
    selectedColumns.map(column => column.getValue(row, terminalHeader))
  ))
  const summaryRows = [
    ["report", "cable-schedule"],
    ["workspace_name", workspaceName],
    ["exported_at", generatedAt.toISOString()],
    ["total", String(allocatedCableRows.value.length)],
  ]

  const csvContent = [
    ...summaryRows.map(row => row.map(csvEscape).join(",")),
    "",
    tableHeaders.map(csvEscape).join(","),
    ...rows.map(row => row.map(csvEscape).join(",")),
  ].join("\n")

  const dateSuffix = generatedAt.toISOString().slice(0, 19).replace(/:/g, "-")
  const filename = `cable-schedule-ws-${workspaceFilePart}-${dateSuffix}.csv`
  downloadTextFile(csvContent, filename)
  toastStore.success(`Cable schedule exported: ${rows.length} rows`)
}

function openExportModal() {
  if (workspaceMissing.value) return
  if (!allocatedCableRows.value.length) {
    toastStore.info("No allocated rows to export.")
    return
  }
  exportModalOpen.value = true
}

function closeExportModal() {
  exportModalOpen.value = false
}

function handleExportCableFromWizard(payload: { optionalColumnKeys: string[] }) {
  exportModalOpen.value = false
  exportCableJournal(payload.optionalColumnKeys)
}

function exportSignalReport() {
  if (!allocationRows.value.length) {
    toastStore.info("No signals to export.")
    return
  }

  const headers = sourceColumnHeaders.value
  const fallbackHeaders = headers.length > 0 ? headers : ["signal_name", "signal_key"]
  const csvHeaders = [...fallbackHeaders, "signal_direction", "unit_id", "channel_index", "last_tested_at"]
  const rows = buildSignalReportRows()
  const workspaceName = String(workspaceStore.activeWorkspace?.name ?? "")
  const workspaceFilePart = toFilenamePart(workspaceName)
  const generatedAt = new Date()
  const tested = testedSignalsCount.value
  const remaining = remainingSignalsCount.value
  const total = allocationRows.value.length

  const metaRows = [
    ["report", "signal-test-report"],
    ["generated_at", generatedAt.toISOString()],
    ["signals_total", String(total)],
    ["signals_tested", String(tested)],
    ["signals_remaining", String(remaining)],
  ]

  const csvContent = [
    ...metaRows.map(row => row.map(csvEscape).join(",")),
    "",
    csvHeaders.map(csvEscape).join(","),
    ...rows.map(row => row.map(csvEscape).join(",")),
  ].join("\n")

  const dateSuffix = generatedAt.toISOString().slice(0, 19).replace(/:/g, "-")
  const filename = `signal-report-ws-${workspaceFilePart}-${dateSuffix}.csv`
  downloadTextFile(csvContent, filename)
  toastStore.success(`Report exported: tested ${tested}, remaining ${remaining}, total ${total}`)
}

function normalizedChannelType(raw: string | null | undefined): "di" | "do" | "ai" | "ao" | null {
  const value = String(raw || "").trim().toLowerCase()
  if (value.startsWith("di")) return "di"
  if (value.startsWith("do")) return "do"
  if (value.startsWith("ai")) return "ai"
  if (value.startsWith("ao")) return "ao"
  return null
}

function channelOptionsBySignal(signalDirection: string): ChannelOption[] {
  const needed = resolveRuntimeChannelTypeForSignal(signalDirection)
  if (!needed) return []
  return channelOptionsByType.value[needed] ?? []
}

const channelOptionsByType = computed<Record<"di" | "do" | "ai" | "ao", ChannelOption[]>>(() => {
  const grouped: Record<"di" | "do" | "ai" | "ao", ChannelOption[]> = {
    di: [],
    do: [],
    ai: [],
    ao: [],
  }
  const deviceMap = new Map(deviceStore.devices.map(device => [device.id, device] as const))
  const sortedChannels = [...channels.value].sort((a, b) => {
    const onlineA = deviceMap.get(a.device_id)?.status === "online" ? 0 : 1
    const onlineB = deviceMap.get(b.device_id)?.status === "online" ? 0 : 1
    if (onlineA !== onlineB) return onlineA - onlineB
    if (a.device_id !== b.device_id) return a.device_id - b.device_id
    return a.index - b.index
  })

  sortedChannels.forEach((channel) => {
    const type = normalizedChannelType(channel.type)
    if (!type) return
    const unitId = channelStore.resolveUnitId(channel.device_id) || `Device ${channel.device_id}`
    grouped[type].push({
      id: channel.id,
      label: `${unitId}/ch${channel.index + 1}`,
      disabled: false,
    })
  })
  return grouped
})

function channelOptionsForRow(row: SignalAllocationRow): ChannelOption[] {
  const options = channelOptionsBySignal(row.signal_direction).map((option) => {
    const ownerSignalId = signalSheetStore.getAllocationOwnerSignalId(option.id)
    const allocatedToAnotherSignal = ownerSignalId !== null && ownerSignalId !== row.signal_id
    return {
      ...option,
      disabled: option.disabled || allocatedToAnotherSignal,
    }
  })

  const currentChannelId = Number(row.channel_id)
  if (Number.isFinite(currentChannelId) && currentChannelId > 0 && !options.some(option => option.id === currentChannelId)) {
    const currentChannel = channelMap.value.get(currentChannelId)
    if (currentChannel) {
      const unitId = channelStore.resolveUnitId(currentChannel.device_id) || `Device ${currentChannel.device_id}`
      options.unshift({
        id: currentChannel.id,
        label: `${unitId}/ch${currentChannel.index + 1}`,
        disabled: false,
      })
    }
  }

  return options
}

function buildChannelGroupsForRow(row: SignalAllocationRow): ChannelOptionGroup[] {
  const groups = new Map<string, ChannelOption[]>()
  channelOptionsForRow(row).forEach((option) => {
    const unitId = channelUnitById.value.get(option.id) ?? "Unassigned unit"
    const group = groups.get(unitId)
    if (group) {
      group.push(option)
      return
    }
    groups.set(unitId, [option])
  })

  return Array.from(groups.entries())
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([unitId, options]) => ({ unitId, options }))
}

function channelGroupsForRow(row: SignalAllocationRow): ChannelOptionGroup[] {
  return buildChannelGroupsForRow(row)
}

function channelGroupsResolverForRow(row: SignalAllocationRow): () => ChannelOptionGroup[] {
  const signalId = Number(row.signal_id)
  let resolver = channelGroupsResolverBySignalId.get(signalId)
  if (resolver) {
    return resolver
  }
  resolver = () => {
    const currentRow = findAllocationRowBySignalId(signalId)
    if (!currentRow) {
      return []
    }
    return channelGroupsForRow(currentRow)
  }
  channelGroupsResolverBySignalId.set(signalId, resolver)
  return resolver
}

function setAllocationForRow(row: SignalAllocationRow, nextChannelId: number | null) {
  // Defer heavy reactive updates out of the click task to keep menu interactions responsive.
  setTimeout(() => {
    void signalSheetStore
      .setAllocation(row.signal_id, Number.isFinite(nextChannelId as number) ? nextChannelId : null)
      .catch((err) => {
        toastStore.error(err instanceof Error ? err.message : String(err))
      })
  }, 0)
}

function handleAllocationPickerSelect(row: SignalAllocationRow, channelId: number | null) {
  if (Number.isFinite(channelId as number)) {
    const channel = channelMap.value.get(Number(channelId))
    const selectedType = normalizedChannelType(channel?.type)
    const neededType = resolveRuntimeChannelTypeForSignal(row.signal_direction)
    if (neededType && selectedType && selectedType !== neededType) {
      toastStore.error(`Invalid mapping: ${row.signal_direction} must be allocated to ${neededType.toUpperCase()} channel.`)
      return
    }
  }
  setAllocationForRow(row, channelId)
}

function allocationDisplayLabel(row: SignalAllocationRow): string {
  if (Number.isFinite(row.channel_index as number)) {
    const channelSuffix = `ch${Number(row.channel_index) + 1}`
    const unitId = String(row.unit_id ?? "").trim()
    return unitId ? `${unitId}/${channelSuffix}` : channelSuffix
  }
  if (row.channel_label && row.channel_label.trim().length > 0) {
    return row.channel_label
  }
  return "—"
}

function allocationValueClass(row: SignalAllocationRow) {
  if (!row.channel_id) return "text-neutral-500 dark:text-neutral-400"
  if (row.unit_online) return "font-semibold text-neutral-900 dark:text-neutral-100"
  return "font-semibold text-neutral-400 dark:text-neutral-500"
}

type ControlTarget = {
  deviceId: number
  unitId: string
  channelIndex: number
  channel: DoChannel
  online: boolean
}

type SendControlOptions = {
  quiet?: boolean
}

function resolveControlTarget(row: SignalAllocationRow): ControlTarget | null {
  if (!Number.isFinite(row.channel_id as number)) {
    return null
  }

  const linkedChannel = channelMap.value.get(Number(row.channel_id))
  if (!linkedChannel || normalizedChannelType(linkedChannel.type) !== "do") {
    return null
  }

  const device = deviceStore.devices.find((item) => item.id === linkedChannel.device_id)
  if (!device) {
    return null
  }

  return {
    deviceId: device.id,
    unitId: device.unit_id,
    channelIndex: linkedChannel.index,
    channel: linkedChannel as DoChannel,
    online: device.status === "online",
  }
}

function canControl(row: SignalAllocationRow) {
  return resolveControlTarget(row) !== null
}

function controlBusy(row: SignalAllocationRow): boolean {
  const target = resolveControlTarget(row)
  if (!target) return false
  const stage = target.channel.ui?.stage ?? "idle"
  return stage === "pending" || stage === "debounce"
}

function controlStateLabel(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "UNKNOWN"
  const stableLabel = target.channel.state ? "ON" : "OFF"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return `${stableLabel} (OFFLINE)`
  if (stage === "pending" || stage === "debounce") return `${stableLabel} (PENDING)`
  if (stage === "error") return `${stableLabel} (ERROR)`
  return stableLabel
}

function controlLampClass(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "bg-neutral-400 dark:bg-neutral-600"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return "bg-neutral-500 dark:bg-neutral-700"
  if (stage === "pending" || stage === "debounce") return "bg-amber-400 animate-pulse"
  if (stage === "error") return "bg-red-500 animate-pulse"
  return target.channel.state ? "bg-emerald-500" : "bg-neutral-400 dark:bg-neutral-600"
}

function controlStatusTag(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "N/A"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return "OFFL"
  if (stage === "pending" || stage === "debounce") return "PEND"
  if (stage === "error") return "ERR"
  return target.channel.state ? "ON" : "OFF"
}

function controlStatusClass(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target || !target.online) return "text-neutral-500 dark:text-neutral-400"
  const stage = target.channel.ui?.stage ?? "idle"
  if (stage === "pending" || stage === "debounce") return "text-amber-600 dark:text-amber-300"
  if (stage === "error") return "text-red-600 dark:text-red-300"
  return target.channel.state
    ? "text-emerald-600 dark:text-emerald-300"
    : "text-neutral-500 dark:text-neutral-300"
}

function controlDisabled(row: SignalAllocationRow): boolean {
  const target = resolveControlTarget(row)
  if (!target) return true
  return !target.online
}

function handleControlMenuSelect(row: SignalAllocationRow, state: boolean) {
  void sendControl(row, state)
}

function waitForControlResult(target: ControlTarget, expectedState: boolean, timeoutMs = 2600): Promise<boolean> {
  const startedAt = Date.now()
  return new Promise((resolve) => {
    const poll = () => {
      const stage = target.channel.ui?.stage ?? "idle"
      if (stage === "error") {
        resolve(false)
        return
      }
      if (stage === "idle" && Boolean(target.channel.state) === expectedState) {
        resolve(true)
        return
      }
      if (Date.now() - startedAt >= timeoutMs) {
        resolve(false)
        return
      }
      setTimeout(poll, 60)
    }
    poll()
  })
}

async function sendControl(row: SignalAllocationRow, state: boolean, options: SendControlOptions = {}): Promise<boolean> {
  const target = resolveControlTarget(row)
  if (!target) {
    toastStore.error("Channel not found")
    return false
  }
  if (!target.online) {
    toastStore.error("Device is offline")
    return false
  }
  if (controlBusy(row) && target.channel.ui?.target === state) {
    return false
  }

  try {
    channelStore.sendDoCommand(target.unitId, target.channelIndex, state)
    const succeeded = await waitForControlResult(target, state)
    if (!succeeded) {
      if (!options.quiet) {
        toastStore.warning("Command not confirmed by device")
      }
      return false
    }
    void signalSheetStore.markSignalsTested([row.signal_id]).catch(() => {
      return
    })
    return true
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
    return false
  }
}

function handleRowClick() {
  return
}

function handleSelectionChange(payload: { rowKeys: string[] }) {
  selectedRowKeys.value = payload.rowKeys
}

function openImportModal() {
  if (workspaceMissing.value) return
  importModalOpen.value = true
}

function closeImportModal() {
  importModalOpen.value = false
}

async function handleImported() {
  importModalOpen.value = false
  await refreshAll()
  await signalSheetStore.refreshPresets()
}

function isImportQueryRequested(raw: unknown): boolean {
  const values = Array.isArray(raw) ? raw : [raw]
  return values.some((value) => {
    const normalized = String(value ?? "").trim().toLowerCase()
    return normalized === "1" || normalized === "true" || normalized === "yes" || normalized === "open"
  })
}

function clearImportQueryFlag() {
  if (!("import" in route.query)) return
  const nextQuery = { ...route.query }
  delete nextQuery.import
  void router.replace({ query: nextQuery }).catch(() => {
    return
  })
}

async function awaitUiPaintFrame() {
  await nextTick()
  await new Promise<void>((resolve) => {
    if (typeof requestAnimationFrame === "function") {
      requestAnimationFrame(() => resolve())
      return
    }
    setTimeout(() => resolve(), 0)
  })
}

function readNumericResult(job: SignalAllocationJob, key: string): number {
  const raw = (job.result as Record<string, unknown> | undefined)?.[key]
  const numeric = Number(raw)
  return Number.isFinite(numeric) ? numeric : 0
}

function formatTestRunSkipReasons(job: SignalAllocationJob): string {
  const reasonsRaw = (job.result as Record<string, unknown> | undefined)?.skip_reasons
  if (!reasonsRaw || typeof reasonsRaw !== "object") {
    return ""
  }

  const reasons = reasonsRaw as Record<string, unknown>
  const offline = Number(reasons.offline_unit ?? 0)
  const nonDo = Number(reasons.non_do_channel ?? 0)
  const invalidBinding = Number(reasons.invalid_binding ?? 0)
  const missingRow = Number(reasons.missing_row ?? 0)

  const parts: string[] = []
  if (Number.isFinite(offline) && offline > 0) {
    parts.push(`${offline} offline`)
  }
  if (Number.isFinite(nonDo) && nonDo > 0) {
    parts.push(`${nonDo} not DO`)
  }
  if (Number.isFinite(invalidBinding) && invalidBinding > 0) {
    parts.push(`${invalidBinding} invalid binding`)
  }
  if (Number.isFinite(missingRow) && missingRow > 0) {
    parts.push(`${missingRow} missing`)
  }

  return parts.join(", ")
}

async function allocateSelectedUnassigned() {
  if (allocatingSelected.value) return
  if (!selectedUnassignedSignalIds.value.length) return
  const targetSignalIds = [...selectedAllocatableUnassignedSignalIds.value]
  if (!targetSignalIds.length) {
    toastStore.info("No free compatible channels available for selected rows.")
    return
  }
  allocatingSelected.value = true
  await awaitUiPaintFrame()
  try {
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) {
      return
    }

    const { data } = await SignalSheetAPI.autoAllocate(workspaceId, {
      signal_ids: targetSignalIds,
      prefer_online: true,
      overwrite_existing: false,
    })
    await signalSheetStore.refreshAllocations()

    const assigned = Number(data.result?.assigned ?? 0)
    const restRaw = data.result?.unassigned_signal_ids
    const rest = Array.isArray(restRaw) ? restRaw.length : 0
    const unavailableSkipped = Math.max(0, selectedUnassignedSignalIds.value.length - targetSignalIds.length)
    toastStore.success(
      `Allocation complete: ${assigned} assigned`
      + `${rest ? `, ${rest} left unassigned` : ""}`
      + `${unavailableSkipped ? `, ${unavailableSkipped} skipped (no free channel)` : ""}`,
    )
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    allocatingSelected.value = false
  }
}

async function deallocateSelected() {
  if (deallocatingSelected.value) return
  if (!selectedAllocatedSignalIds.value.length) return
  deallocatingSelected.value = true
  await awaitUiPaintFrame()
  try {
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) {
      return
    }

    await SignalSheetAPI.updateAllocations(
      workspaceId,
      selectedAllocatedSignalIds.value.map(signalId => ({ signal_id: signalId, channel_id: null })),
    )
    await signalSheetStore.refreshAllocations()

    toastStore.success(`Unassigned ${selectedAllocatedSignalIds.value.length} selected signal(s)`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    deallocatingSelected.value = false
  }
}

function updateTestRunStatsFromJob(job: SignalAllocationJob) {
  const total = Math.max(0, Number(job.progress_total ?? testRunTotal.value ?? 0))
  const done = Math.max(0, Math.min(total || Number.MAX_SAFE_INTEGER, Number(job.progress_done ?? 0)))
  testRunTotal.value = total || testRunTotal.value
  testRunProcessed.value = done

  const succeeded = readNumericResult(job, "succeeded")
  const skipped = readNumericResult(job, "skipped")
  if (succeeded > 0 || skipped > 0 || job.status === "succeeded") {
    testRunSucceeded.value = succeeded
    testRunSkipped.value = skipped
    return
  }

  const message = String(job.message ?? "")
  const okMatch = message.match(/ok\s+(\d+)/i)
  const skipMatch = message.match(/skip\s+(\d+)/i)
  if (okMatch) {
    testRunSucceeded.value = Number(okMatch[1])
  }
  if (skipMatch) {
    testRunSkipped.value = Number(skipMatch[1])
  }
}

function setTestRunToggleMode(mode: "single" | "double") {
  testRunToggleMode.value = mode
}

function setTestRunIntervalMs(intervalMs: number) {
  const normalized = Math.max(100, Math.min(10000, Number(intervalMs)))
  testRunIntervalMs.value = normalized
}

async function controlActiveTestRun(action: "pause" | "resume" | "stop") {
  if (testRunControlBusy.value) return
  const workspaceId = workspaceStore.activeWorkspaceId
  const jobId = activeTestRunJob.value?.job_id
  if (!workspaceId || !jobId) {
    return
  }

  testRunControlBusy.value = true
  try {
    await signalJobStore.controlJob(workspaceId, jobId, action)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  } finally {
    testRunControlBusy.value = false
  }
}

async function runTestVisualOnly() {
  if (canResumeActiveTestRun.value) {
    await controlActiveTestRun("resume")
    return
  }
  if (testRunInProgress.value || Boolean(activeTestRunJob.value)) return

  const queue = selectedAllocatedPhysicalRows.value.filter(row => canControl(row))
  if (!queue.length) {
    toastStore.info("Selected rows have no controllable DO channels.")
    return
  }

  testRunInProgress.value = true
  testRunTotal.value = queue.length
  testRunProcessed.value = 0
  testRunSucceeded.value = 0
  testRunSkipped.value = 0
  testRunStartedAtMs.value = Date.now()
  try {
    const workspaceId = workspaceStore.activeWorkspaceId
    if (!workspaceId) {
      return
    }

    const selectedSignalIds = queue.map(row => row.signal_id)
    const completedJob = await signalJobStore.enqueueTestRunJob(
      workspaceId,
      selectedSignalIds,
      {
        signalIntervalMs: testRunIntervalMs.value,
        toggleMode: testRunToggleMode.value,
      },
    )
    updateTestRunStatsFromJob(completedJob)
    await signalSheetStore.refreshAllocations()
    setLastTestSummary(completedJob, testRunStartedAtMs.value)

    const skipDetails = formatTestRunSkipReasons(completedJob)
    toastStore.success(
      `Run test complete: ${testRunSucceeded.value} toggled`
      + `${testRunSkipped.value ? `, ${testRunSkipped.value} skipped` : ""}`
      + `${skipDetails ? ` (${skipDetails})` : ""}.`,
    )
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err)
    if (message.toLowerCase().includes("cancelled")) {
      toastStore.info("Run test cancelled")
    } else {
      toastStore.error(message)
    }
  } finally {
    testRunStartedAtMs.value = null
    testRunInProgress.value = false
  }
}

function createSwitchgearVisualOnly() {
  void createSwitchgearsFromSelection()
}

function sortRowsByUnitChannel(rows: SignalAllocationRow[]): SignalAllocationRow[] {
  return [...rows].sort((a, b) => {
    const unitA = String(a.unit_id ?? "")
    const unitB = String(b.unit_id ?? "")
    const byUnit = unitA.localeCompare(unitB)
    if (byUnit !== 0) return byUnit
    const indexA = Number.isFinite(a.channel_index as number) ? Number(a.channel_index) : Number.MAX_SAFE_INTEGER
    const indexB = Number.isFinite(b.channel_index as number) ? Number(b.channel_index) : Number.MAX_SAFE_INTEGER
    if (indexA !== indexB) return indexA - indexB
    return a.signal_id - b.signal_id
  })
}

function buildPairsPreferSameUnit(rows: SignalAllocationRow[]): Array<[SignalAllocationRow, SignalAllocationRow]> {
  const queue = sortRowsByUnitChannel(rows)
  const pairs: Array<[SignalAllocationRow, SignalAllocationRow]> = []
  while (queue.length >= 2) {
    let paired = false
    for (let i = 0; i < queue.length - 1; i += 1) {
      const first = queue[i]
      const second = queue[i + 1]
      if (String(first.unit_id ?? "") === String(second.unit_id ?? "")) {
        pairs.push([first, second])
        queue.splice(i, 2)
        paired = true
        break
      }
    }
    if (!paired) {
      const first = queue.shift()
      const second = queue.shift()
      if (!first || !second) break
      pairs.push([first, second])
    }
  }
  return pairs
}

function makeSwitchgearName(index: number, total: number): string {
  const stamp = new Date().toISOString().slice(0, 16).replace("T", " ").replace(":", "-")
  if (total <= 1) {
    return `Signal Switchgear ${stamp}`
  }
  return `Signal Switchgear ${stamp} #${index + 1}`
}

async function createSwitchgearsFromSelection() {
  if (!switchgearCreatableCount.value) return
  if (switchgearCreateInProgress.value) return

  switchgearCreateInProgress.value = true
  const selectedSignalIds = selectedSwitchgearRows.value.map(row => row.signal_id)
  const requestedCount = switchgearCreatableCount.value
  const unallocatedSignalIds = selectedSwitchgearRows.value
    .filter(row => !Number.isFinite(row.channel_id as number))
    .map(row => row.signal_id)

  try {
    if (unallocatedSignalIds.length > 0) {
      try {
        const result = await signalSheetStore.autoAllocate({
          signal_ids: unallocatedSignalIds,
          prefer_online: true,
          prefer_single_unit: true,
          overwrite_existing: false,
        })
        if (result.result.unassigned_signal_ids.length > 0) {
          toastStore.warning(
            `Not enough free channels: ${result.result.unassigned_signal_ids.length} signal(s) still unassigned.`,
          )
        }
      } catch (err) {
        toastStore.error(err instanceof Error ? err.message : String(err))
        return
      }
    }

    const selectedSet = new Set(selectedSignalIds)
    const resolvedRows = signalSheetStore.allocationRows.filter((row) => {
      if (!selectedSet.has(row.signal_id)) return false
      if (!Number.isFinite(row.channel_id as number)) return false
      if (!Number.isFinite(row.channel_index as number)) return false
      if (!String(row.unit_id ?? "").trim()) return false
      return row.signal_direction === "DI" || row.signal_direction === "DO"
    })

    const diRows = resolvedRows.filter(row => row.signal_direction === "DI")
    const doRows = resolvedRows.filter(row => row.signal_direction === "DO")
    const diPairs = buildPairsPreferSameUnit(diRows)
    const doPairs = buildPairsPreferSameUnit(doRows)
    const creatableNow = Math.min(requestedCount, diPairs.length, doPairs.length)

    if (creatableNow <= 0) {
      toastStore.warning("No complete 2DI+2DO sets available after allocation.")
      return
    }

    let created = 0
    for (let index = 0; index < creatableNow; index += 1) {
      const doPair = diPairs[index] // DI signals are allocated to DO channels (commands)
      const diPair = doPairs[index] // DO signals are allocated to DI channels (feedback)

      const doOpen = Number(doPair[0].channel_id)
      const doClosed = Number(doPair[1].channel_id)
      const diOpen = Number(diPair[0].channel_id)
      const diClose = Number(diPair[1].channel_id)
      if (!Number.isFinite(doOpen) || !Number.isFinite(doClosed) || !Number.isFinite(diOpen) || !Number.isFinite(diClose)) {
        continue
      }

      try {
        await switchgearStore.create({
          name: makeSwitchgearName(index, creatableNow),
          switchgear_type: "switchgear",
          bindings: [
            { role: "do_open", channel_id: doOpen, delay_ms: 0 },
            { role: "do_closed", channel_id: doClosed, delay_ms: 0 },
            { role: "di_open", channel_id: diOpen, delay_ms: 0 },
            { role: "di_close", channel_id: diClose, delay_ms: 0 },
          ],
        })
        created += 1
      } catch (err) {
        toastStore.error(err instanceof Error ? err.message : String(err))
        break
      }
    }

    if (created <= 0) {
      return
    }

    if (created < requestedCount) {
      toastStore.warning(`Created ${created} of ${requestedCount} switchgears (not enough paired channels).`)
    } else {
      const noun = created === 1 ? "switchgear" : "switchgears"
      toastStore.success(`Created ${created} ${noun} from selected signals.`)
    }
  } finally {
    switchgearCreateInProgress.value = false
  }
}

async function ensureRuntimeCatalogLoaded() {
  await deviceStore.ensureLoaded()
  await channelStore.ensureLoaded()

  if (channels.value.length === 0 && deviceStore.devices.length > 0) {
    await Promise.allSettled(
      deviceStore.devices.map(device => channelStore.ensureDeviceChannelsLoaded(device.id)),
    )
  }
}

async function refreshAll() {
  try {
    await Promise.all([
      signalSheetStore.refreshSheet(),
      signalSheetStore.refreshAllocations(),
      ensureRuntimeCatalogLoaded(),
    ])
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

watch(
  sourceColumnHeaders,
  () => {
    rebuildGridRows()
  },
  { immediate: true },
)

watch(
  () => allocationRevision.value,
  () => {
    const signalIds = recentlyChangedSignalIds.value
    if (isTestRunBusy.value) {
      queueTestedAtOverlay(signalIds)
      return
    }
    if (!signalIds.length) {
      rebuildGridRows()
      return
    }
    syncGridRowsBySignalIds(signalIds)
  },
  { immediate: true, flush: "post" },
)

function syncRealtimeUnitScope() {
  const units = new Set<string>()
  allocationRows.value.forEach((row) => {
    if (!row.channel_id) return
    const channel = channelMap.value.get(row.channel_id)
    if (!channel) return
    const unitId = channelUnitById.value.get(channel.id) ?? null
    if (unitId) units.add(unitId)
  })

  const scopedUnits = [...units]
  realtimeScopeStore.setRealtimeUnitScope(scopeId, scopedUnits)

  const scopeKey = scopedUnits.slice().sort((left, right) => left.localeCompare(right)).join("|")
  if (scopeKey === lastRealtimeScopeKey) {
    return
  }
  lastRealtimeScopeKey = scopeKey

  const deviceByUnit = new Map(deviceStore.devices.map(device => [device.unit_id, device.id] as const))
  for (const unitId of scopedUnits) {
    const deviceId = deviceByUnit.get(unitId)
    if (!Number.isFinite(deviceId as number)) {
      continue
    }
    channelStore.requestStates(Number(deviceId), { includeDiagnostics: false, silent: true })
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

watch(
  () => workspaceStore.activeWorkspaceId,
  async (workspaceId) => {
    if (!workspaceId) return
    signalSheetStore.resetState()
    restoreSelectedRowKeysFromStorage()
    await refreshAll()
  },
  { immediate: true },
)

watch(
  selectedRowKeys,
  () => {
    persistSelectedRowKeysToStorage()
  },
  { deep: false },
)

watch(
  () => allocationRevision.value,
  () => {
    if (isTestRunBusy.value) {
      return
    }
    scheduleRealtimeUnitScopeSync()
  },
  { immediate: true, flush: "post" },
)

watch(
  () => [channels.value.length, deviceStore.devices.length, workspaceStore.activeWorkspaceId],
  () => {
    scheduleRealtimeUnitScopeSync()
  },
  { immediate: true, flush: "post" },
)

watch(
  () => [route.query.import, workspaceMissing.value, loading.value] as const,
  ([importFlag, missingWorkspace, isLoading]) => {
    if (!isImportQueryRequested(importFlag)) return
    if (missingWorkspace || isLoading) return
    openImportModal()
    clearImportQueryFlag()
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  clearTestedAtOverlay()
  channelGroupsResolverBySignalId.clear()
  if (realtimeScopeSyncFrame !== null) {
    cancelAnimationFrame(realtimeScopeSyncFrame)
    realtimeScopeSyncFrame = null
  }
  realtimeScopeStore.clearRealtimeUnitScope(scopeId)
})
</script>
