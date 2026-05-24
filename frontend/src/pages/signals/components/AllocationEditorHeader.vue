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
            📥 Import Signal List
          </UiButton>

          <UiButton
            variant="secondary"
            size="sm"
            :disabled="workspaceMissing || loading || allocatedCableRowsCount === 0"
            @click="emit('exportCable')"
          >
            📤 Export Cable Schedule
          </UiButton>

          <UiButton
            variant="secondary"
            size="sm"
            :disabled="workspaceMissing || loading || allocationRowsCount === 0"
            @click="emit('exportReport')"
          >
            📤 Export Report
          </UiButton>
        </div>

        <div class="flex flex-wrap items-center justify-end gap-2">
          <UiButton
            v-if="canAllocateSelected"
            variant="secondary"
            size="sm"
            :disabled="loading || allocatingSelected || deallocatingSelected"
            @click="emit('allocateSelected')"
          >
            {{ allocateSelectedLabel }}
          </UiButton>

          <UiButton
            v-if="canDeallocateSelected"
            variant="secondary"
            size="sm"
            :disabled="loading || allocatingSelected || deallocatingSelected"
            @click="emit('deallocateSelected')"
          >
            {{ deallocateSelectedLabel }}
          </UiButton>

          <span v-if="canRunTest" class="inline-flex items-center gap-2">
            <UiMenu>
              <div class="inline-flex overflow-hidden rounded-lg border border-emerald-500/30 bg-emerald-500/10 divide-x divide-emerald-500/30 shadow-sm shadow-emerald-500/25 dark:border-emerald-400/40 dark:bg-emerald-400/10 dark:divide-emerald-300/30">
                <UiButton
                  :variant="'success'"
                  size="sm"
                  class="flex items-center gap-2 rounded-none px-3 text-sm font-semibold tracking-tight"
                  :disabled="loading || isTestRunBusy"
                  @click="emit('runTest')"
                >
                  {{ canResumeActiveTestRun ? "Resume" : (isTestRunBusy ? "Running…" : "Run test") }}
                </UiButton>
                <UiMenuTrigger as-child>
                  <UiButton
                    :variant="'success'"
                    size="sm"
                    class="rounded-none px-2.5 text-base font-semibold"
                    :disabled="loading || isTestRunBusy"
                    aria-label="Test run options"
                  >
                    <span aria-hidden="true">▾</span>
                  </UiButton>
                </UiMenuTrigger>
              </div>
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
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
} from "@/components/ui/menu"

import UiButton from "@/components/ui/UiButton.vue"

const props = defineProps<{
  summaryText: string
  workspaceMissing: boolean
  loading: boolean
  allocatedCableRowsCount: number
  allocationRowsCount: number
  allocatingSelected: boolean
  deallocatingSelected: boolean
  allocateSelectedLabel: string
  deallocateSelectedLabel: string
  canResumeActiveTestRun: boolean
  canAllocateSelected: boolean
  canDeallocateSelected: boolean
  canRunTest: boolean
  isTestRunBusy: boolean
  testRunToggleMode: "single" | "double"
  testRunIntervalMs: number
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
}>()
</script>
