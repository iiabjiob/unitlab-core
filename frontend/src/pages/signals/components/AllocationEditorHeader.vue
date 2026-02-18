<template>
  <header class="rounded-2xl border border-neutral-200 bg-white px-4 py-3 dark:border-neutral-800 dark:bg-neutral-900">
    <div class="grid gap-3 md:grid-cols-[minmax(0,1fr)_auto] md:items-start">
      <div class="min-w-0">
        <p class="text-xs font-semibold uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">Live Signal Sheet</p>
        <p class="mt-1 text-sm text-neutral-700 dark:text-neutral-200">
          {{ summaryText }}
        </p>
        <div class="mt-2 flex flex-wrap items-center gap-2">
          <UiButton variant="primary" size="sm" :disabled="workspaceMissing || loading" @click="emit('import')">
            + Import Signal List
          </UiButton>
          <UiButton
            variant="secondary"
            size="sm"
            :disabled="workspaceMissing || loading || allocatedCableRowsCount === 0"
            @click="emit('exportCable')"
          >
            Export Cable Schedule
          </UiButton>
          <UiButton
            variant="secondary"
            size="sm"
            :disabled="workspaceMissing || loading || allocationRowsCount === 0"
            @click="emit('exportReport')"
          >
            Export Report
          </UiButton>
        </div>
      </div>

      <div
        v-if="showRightPanel"
        class="grid gap-2 rounded-lg border border-neutral-200 bg-neutral-50/70 px-2.5 py-2 dark:border-neutral-700 dark:bg-neutral-800/50"
      >
        <div class="flex flex-wrap items-center gap-2">
          <span class="text-[11px] font-semibold uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400">Allocation</span>
          <div class="ml-auto flex flex-wrap items-center justify-end gap-2">
            <div
              v-if="(updatingAllocations || allocatingSelected || deallocatingSelected || allocationJobsRunning)"
              class="inline-flex items-center gap-1.5 rounded-md border border-neutral-200 bg-white px-2 py-0.5 text-[11px] font-medium text-neutral-600 dark:border-neutral-700 dark:bg-neutral-900/60 dark:text-neutral-200"
            >
              <span class="h-1.5 w-1.5 animate-pulse rounded-full bg-emerald-500"></span>
              <span>{{ allocationJobProgressText || "Applying changes…" }}</span>
            </div>
            <InlineInfoTooltip
              v-if="canCreateSwitchgearFromSelection"
              text="Create switchgear items from selected DI/DO signal pairs."
              :disabled="isTestRunMenuOpen"
              placement="bottom"
              align="end"
              :open-delay="1000"
              v-slot="{ setTriggerRef, getTriggerProps }"
            >
              <span :ref="setTriggerRef" v-bind="getTriggerProps()" class="inline-flex">
                <UiButton
                  variant="secondary"
                  size="sm"
                  :disabled="loading || switchgearCreateInProgress"
                  @click="emit('createSwitchgear')"
                >
                  {{ switchgearCreateInProgress ? "Creating…" : createSwitchgearButtonLabel }}
                </UiButton>
              </span>
            </InlineInfoTooltip>
            <UiButton
              v-if="canAllocateSelected"
              variant="secondary"
              size="sm"
              :disabled="loading || allocatingSelected"
              @click="emit('allocateSelected')"
            >
              {{ allocatingSelected ? "Allocating…" : "Allocate" }}
            </UiButton>
            <UiButton
              v-if="canDeallocateSelected"
              variant="ghost"
              size="sm"
              :disabled="loading || deallocatingSelected"
              @click="emit('deallocateSelected')"
            >
              {{ deallocatingSelected ? "Unassigning…" : "Unassign" }}
            </UiButton>
          </div>
        </div>

        <div class="flex flex-wrap items-center gap-2 border-t border-neutral-200 pt-2 dark:border-neutral-700">
          <span class="text-[11px] font-semibold uppercase tracking-[0.08em] text-neutral-500 dark:text-neutral-400">Test run</span>
          <div class="ml-auto flex flex-wrap items-center justify-end gap-2">
            <div
              v-if="lastTestSummaryText"
              class="flex items-center gap-2 rounded-md border border-neutral-200 bg-white px-2 py-1 text-xs text-neutral-700 dark:border-neutral-700 dark:bg-neutral-900/60 dark:text-neutral-200"
            >
              <span class="truncate">Last test · {{ lastTestSummaryText }}</span>
              <button
                type="button"
                class="inline-flex h-4 w-4 items-center justify-center rounded text-neutral-500 hover:bg-neutral-200 hover:text-neutral-800 dark:text-neutral-400 dark:hover:bg-neutral-700 dark:hover:text-neutral-100"
                @click="emit('dismissLastTest')"
              >
                ×
              </button>
            </div>

            <div
              v-if="showTestRunProgress"
              class="min-w-[220px] rounded-md border border-neutral-200 bg-white px-2 py-1 dark:border-neutral-700 dark:bg-neutral-900/60"
            >
              <div class="flex items-center justify-between text-[11px] font-medium text-neutral-600 dark:text-neutral-300">
                <span>{{ testRunProgressText }}</span>
                <div class="inline-flex items-center gap-1">
                  <span>{{ testRunProgressPercent }}%</span>
                  <UiButton
                    v-if="canPauseActiveTestRun"
                    size="xs"
                    variant="ghost"
                    :disabled="testRunControlBusy"
                    title="Pause test run"
                    @click="emit('controlTestRun', 'pause')"
                  >
                    ⏸
                  </UiButton>
                  <UiButton
                    v-if="canResumeActiveTestRun"
                    size="xs"
                    variant="ghost"
                    :disabled="testRunControlBusy"
                    title="Resume test run"
                    @click="emit('controlTestRun', 'resume')"
                  >
                    ▶
                  </UiButton>
                  <UiButton
                    v-if="canStopActiveTestRun"
                    size="xs"
                    variant="ghost"
                    :disabled="testRunControlBusy || isStoppingActiveTestRun"
                    title="Stop test run"
                    @click="emit('controlTestRun', 'stop')"
                  >
                    ■
                  </UiButton>
                </div>
              </div>
              <div class="mt-1 h-1.5 overflow-hidden rounded bg-neutral-200 dark:bg-neutral-700">
                <div
                  class="h-full bg-emerald-500 transition-[width] duration-200"
                  :style="{ width: `${testRunProgressPercent}%` }"
                ></div>
              </div>
            </div>

            <span v-if="canRunTest" class="inline-flex">
              <UiMenu ref="testRunMenuRef">
                <UiButton
                  :variant="'success'"
                  size="sm"
                  :disabled="loading || isTestRunBusy"
                  @click="emit('runTest')"
                  @contextmenu.capture.prevent.stop="openTestRunContextMenu"
                >
                  {{ isTestRunBusy ? "Running…" : "Run test" }}
                </UiButton>
                <UiMenuContent>
                  <UiMenuLabel>
                    Toggle mode
                  </UiMenuLabel>
                  <UiMenuItem @select="emit('setToggleMode', 'single')">
                    Single toggle (Invert state)
                    <span v-if="testRunToggleMode === 'single'" class="ml-2 text-xs">✓</span>
                  </UiMenuItem>
                  <UiMenuItem @select="emit('setToggleMode', 'double')">
                    Double toggle (Invert → Return)
                    <span v-if="testRunToggleMode === 'double'" class="ml-2 text-xs">✓</span>
                  </UiMenuItem>
                  <UiMenuSeparator />
                  <UiMenuLabel>
                    Interval between signals
                  </UiMenuLabel>
                  <UiMenuItem @select="emit('setIntervalMs', 500)">
                    0.5 s
                    <span v-if="testRunIntervalMs === 500" class="ml-2 text-xs">✓</span>
                  </UiMenuItem>
                  <UiMenuItem @select="emit('setIntervalMs', 1000)">
                    1.0 s
                    <span v-if="testRunIntervalMs === 1000" class="ml-2 text-xs">✓</span>
                  </UiMenuItem>
                  <UiMenuItem @select="emit('setIntervalMs', 2000)">
                    2.0 s
                    <span v-if="testRunIntervalMs === 2000" class="ml-2 text-xs">✓</span>
                  </UiMenuItem>
                </UiMenuContent>
              </UiMenu>
            </span>
          </div>
        </div>
      </div>
    </div>
  </header>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
  type MenuController,
} from "@affino/menu-vue"

import UiButton from "@/components/ui/UiButton.vue"

const props = defineProps<{
  summaryText: string
  workspaceMissing: boolean
  loading: boolean
  allocatedCableRowsCount: number
  allocationRowsCount: number
  updatingAllocations: boolean
  allocatingSelected: boolean
  deallocatingSelected: boolean
  allocationJobsRunning: boolean
  allocationJobProgressText: string | null
  showTestRunProgress: boolean
  testRunProgressText: string
  testRunProgressPercent: number
  canPauseActiveTestRun: boolean
  canResumeActiveTestRun: boolean
  canStopActiveTestRun: boolean
  testRunControlBusy: boolean
  isStoppingActiveTestRun: boolean
  lastTestSummaryText: string
  canAllocateSelected: boolean
  canDeallocateSelected: boolean
  canRunTest: boolean
  isTestRunBusy: boolean
  testRunToggleMode: "single" | "double"
  testRunIntervalMs: number
  canCreateSwitchgearFromSelection: boolean
  switchgearCreateInProgress: boolean
  createSwitchgearButtonLabel: string
}>()

const emit = defineEmits<{
  (event: "import"): void
  (event: "exportCable"): void
  (event: "exportReport"): void
  (event: "allocateSelected"): void
  (event: "deallocateSelected"): void
  (event: "runTest"): void
  (event: "setToggleMode", mode: "single" | "double"): void
  (event: "setIntervalMs", intervalMs: number): void
  (event: "controlTestRun", action: "pause" | "resume" | "stop"): void
  (event: "dismissLastTest"): void
  (event: "createSwitchgear"): void
}>()

const testRunMenuRef = ref<{ controller?: MenuController } | null>(null)
const isTestRunMenuOpen = computed(() => Boolean(testRunMenuRef.value?.controller?.state.open))
const showAllocationProgress = computed(() => (
  (props.updatingAllocations || props.allocatingSelected || props.deallocatingSelected || props.allocationJobsRunning)
))
const showRightPanel = computed(() => (
  props.canCreateSwitchgearFromSelection
  || props.canAllocateSelected
  || props.canDeallocateSelected
  || props.canRunTest
  || props.showTestRunProgress
  || showAllocationProgress.value
  || Boolean(props.lastTestSummaryText)
))

function openTestRunContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const controller = testRunMenuRef.value?.controller
  if (!controller) {
    return
  }
  if (controller.state.open) {
    return
  }
  controller.setAnchor({ x: event.clientX, y: event.clientY, width: 0, height: 0 })
  controller.open("pointer")
}
</script>
