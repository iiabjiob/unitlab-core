<template>
  <header class="rounded-2xl border border-neutral-200 bg-white px-4 py-3 dark:border-neutral-800 dark:bg-neutral-900">
    <div class="flex flex-col gap-3">
      <div class="min-w-0">
        <p class="text-xs font-semibold uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">Live Signal Sheet</p>
        <p class="mt-1 text-sm text-neutral-700 dark:text-neutral-200">
          {{ summaryText }}
        </p>
      </div>

      <div class="flex w-full flex-wrap items-center justify-between gap-2">
        <div class="flex flex-wrap items-center gap-2">
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

        <div class="flex flex-wrap items-center justify-end gap-2">
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
            {{ allocatingSelected ? "Assigning…" : "Assign Hardware" }}
          </UiButton>

          <UiButton
            v-if="canDeallocateSelected"
            variant="secondary"
            size="sm"
            :disabled="loading || deallocatingSelected"
            @click="emit('deallocateSelected')"
          >
            {{ deallocatingSelected ? "Unassigning…" : "Unassign Hardware" }}
          </UiButton>

          <span v-if="canRunTest" class="inline-flex">
            <UiMenu ref="testRunMenuRef">
              <UiButton
                :variant="'success'"
                size="sm"
                :disabled="loading || isTestRunBusy"
                @click="emit('runTest')"
                @contextmenu.capture.prevent.stop="openTestRunContextMenu"
              >
                {{ canResumeActiveTestRun ? "Resume" : (isTestRunBusy ? "Running…" : "Run test") }}
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
import InlineInfoTooltip from "@/components/ui/InlineInfoTooltip.vue"

const props = defineProps<{
  summaryText: string
  workspaceMissing: boolean
  loading: boolean
  allocatedCableRowsCount: number
  allocationRowsCount: number
  allocatingSelected: boolean
  deallocatingSelected: boolean
  canResumeActiveTestRun: boolean
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
  (event: "createSwitchgear"): void
}>()

const testRunMenuRef = ref<{ controller?: MenuController } | null>(null)
const isTestRunMenuOpen = computed(() => Boolean(testRunMenuRef.value?.controller?.state.open))

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
