<template>
  <header class="allocation-editor-header">
    <div class="allocation-editor-header__stack">
      <div class="allocation-editor-header__copy">
        <p class="allocation-editor-header__eyebrow">Live Signal Sheet</p>
        <p v-if="!signalSummary" class="allocation-editor-header__summary">
          {{ summaryText }}
        </p>
        <div v-else class="allocation-editor-header__live-row">
          <div class="allocation-editor-header__metric-panel" aria-label="Signal list summary">
            <span class="allocation-editor-header__metric-label">Signals</span>
            <span class="allocation-editor-header__metric-item">
              Total {{ signalSummary.total }}
            </span>
            <span class="allocation-editor-header__metric-item">
              Allocated {{ signalSummary.allocated }} <span class="allocation-editor-header__metric-percent">{{ signalSummary.allocatedPercent }}</span>
            </span>
            <span class="allocation-editor-header__metric-item">
              Tested {{ signalSummary.tested }} <span class="allocation-editor-header__metric-percent">{{ signalSummary.testedPercent }}</span>
            </span>
            <span class="allocation-editor-header__metric-item">
              Remaining {{ signalSummary.remaining }} <span class="allocation-editor-header__metric-percent">{{ signalSummary.remainingPercent }}</span>
            </span>
          </div>
          <div
            v-if="iec61850Summary?.active"
            class="allocation-editor-header__metric-panel"
            aria-label="IEC 61850 live status"
          >
            <span class="allocation-editor-header__metric-label">IEC 61850</span>
            <span class="allocation-editor-header__metric-item allocation-editor-header__metric-item--ready">
              <span class="allocation-editor-header__metric-dot" aria-hidden="true"></span>
              Ready {{ iec61850Summary.ready }}
            </span>
            <span class="allocation-editor-header__metric-item allocation-editor-header__metric-item--discovering">
              <span class="allocation-editor-header__metric-dot" aria-hidden="true"></span>
              Discovering {{ iec61850Summary.discovering }}
            </span>
            <span class="allocation-editor-header__metric-item allocation-editor-header__metric-item--offline">
              <span class="allocation-editor-header__metric-dot" aria-hidden="true"></span>
              Offline {{ iec61850Summary.offline }}
            </span>
            <span class="allocation-editor-header__metric-item allocation-editor-header__metric-item--failed">
              <span class="allocation-editor-header__metric-mark" aria-hidden="true">×</span>
              Failed {{ iec61850Summary.failed }}
            </span>
          </div>
        </div>
      </div>

      <div class="allocation-editor-header__actions-row">
        <div class="allocation-editor-header__actions-group">
          <UiButton variant="primary" size="sm" class="allocation-editor-header__button" :disabled="workspaceMissing || loading" @click="emit('import')">
            📥 Import Signal List
          </UiButton>

          <UiButton
            variant="secondary"
            size="sm"
            class="allocation-editor-header__button"
            :disabled="workspaceMissing || loading || allocatedCableRowsCount === 0"
            @click="emit('exportCable')"
          >
            📤 Export Cable Schedule
          </UiButton>

          <UiButton
            variant="secondary"
            size="sm"
            class="allocation-editor-header__button"
            :disabled="workspaceMissing || loading || allocationRowsCount === 0"
            @click="emit('exportReport')"
          >
            📤 Export Report
          </UiButton>
        </div>

        <div class="allocation-editor-header__actions-group allocation-editor-header__actions-group--end">
          <UiButton
            v-if="canAllocateSelected"
            variant="secondary"
            size="sm"
            class="allocation-editor-header__button"
            :disabled="loading || allocatingSelected || deallocatingSelected"
            @click="emit('allocateSelected')"
          >
            {{ allocateSelectedLabel }}
          </UiButton>

          <UiButton
            v-if="canDeallocateSelected"
            variant="secondary"
            size="sm"
            class="allocation-editor-header__button"
            :disabled="loading || allocatingSelected || deallocatingSelected"
            @click="emit('deallocateSelected')"
          >
            {{ deallocateSelectedLabel }}
          </UiButton>

          <span v-if="canRunTest" class="allocation-editor-header__run-wrap">
            <UiMenu>
              <div class="allocation-editor-header__run-segment">
                <UiButton
                  :variant="'success'"
                  size="sm"
                  class="allocation-editor-header__run-button allocation-editor-header__run-button--main"
                  :disabled="loading || isTestRunBusy"
                  @click="emit('runTest')"
                >
                  {{ canResumeActiveTestRun ? "Resume" : (isTestRunBusy ? "Running…" : "Run test") }}
                </UiButton>
                <UiMenuTrigger as-child>
                  <UiButton
                    :variant="'success'"
                    size="sm"
                    class="allocation-editor-header__run-button allocation-editor-header__run-button--trigger"
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
                  <span v-if="testRunToggleMode === 'single'" class="allocation-editor-header__menu-check">✓</span>
                </UiMenuItem>
                <UiMenuItem @select="emit('setToggleMode', 'double')">
                  Double toggle (Invert → Return)
                  <span v-if="testRunToggleMode === 'double'" class="allocation-editor-header__menu-check">✓</span>
                </UiMenuItem>
                <UiMenuSeparator />
                <UiMenuLabel>
                  Interval between signals
                </UiMenuLabel>
                <UiMenuItem @select="emit('setIntervalMs', 500)">
                  0.5 s
                  <span v-if="testRunIntervalMs === 500" class="allocation-editor-header__menu-check">✓</span>
                </UiMenuItem>
                <UiMenuItem @select="emit('setIntervalMs', 1000)">
                  1.0 s
                  <span v-if="testRunIntervalMs === 1000" class="allocation-editor-header__menu-check">✓</span>
                </UiMenuItem>
                <UiMenuItem @select="emit('setIntervalMs', 2000)">
                  2.0 s
                  <span v-if="testRunIntervalMs === 2000" class="allocation-editor-header__menu-check">✓</span>
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
  signalSummary?: {
    total: number
    allocated: number
    allocatedPercent: string
    tested: number
    testedPercent: string
    remaining: number
    remainingPercent: string
  } | null
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
  iec61850Summary?: {
    active: boolean
    ready: number
    discovering: number
    offline: number
    failed: number
  } | null
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

<style scoped>
.allocation-editor-header {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-2xl);
  padding: 0.75rem 1rem;
}

.allocation-editor-header__stack {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.allocation-editor-header__copy {
  min-width: 0;
}

.allocation-editor-header__eyebrow {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0.2em;
  text-transform: uppercase;
}

.allocation-editor-header__summary {
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
  margin-top: 0.25rem;
}

.allocation-editor-header__live-row {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-top: 0.5rem;
}

.allocation-editor-header__metric-panel {
  align-items: center;
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  color: var(--color-neutral-700);
  display: flex;
  flex-wrap: wrap;
  gap: 0.35rem 0.625rem;
  padding: 0.375rem 0.5rem;
  width: fit-content;
}

.allocation-editor-header__metric-label {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 700;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.allocation-editor-header__metric-item {
  align-items: center;
  display: inline-flex;
  font-size: var(--text-xs);
  gap: 0.25rem;
  white-space: nowrap;
}

.allocation-editor-header__metric-percent {
  color: var(--color-neutral-500);
}

.allocation-editor-header__metric-dot {
  border-radius: var(--radius-pill);
  display: inline-block;
  height: 0.45rem;
  width: 0.45rem;
}

.allocation-editor-header__metric-item--ready .allocation-editor-header__metric-dot {
  background: var(--color-emerald-500);
}

.allocation-editor-header__metric-item--discovering .allocation-editor-header__metric-dot {
  background: var(--color-amber-400);
}

.allocation-editor-header__metric-item--offline .allocation-editor-header__metric-dot {
  background: var(--color-neutral-400);
}

.allocation-editor-header__metric-mark {
  color: var(--color-rose-500);
  font-size: var(--text-sm);
  font-weight: 700;
  line-height: 1;
}

.allocation-editor-header__actions-row {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  justify-content: space-between;
  min-height: 2rem;
  width: 100%;
}

.allocation-editor-header__actions-group {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  min-height: 2rem;
}

.allocation-editor-header__actions-group--end {
  justify-content: flex-end;
}

.allocation-editor-header__button {
  height: 2rem;
  white-space: nowrap;
}

.allocation-editor-header__run-wrap {
  align-items: center;
  display: inline-flex;
  gap: 0.5rem;
  height: 2rem;
}

.allocation-editor-header__run-segment {
  border-radius: var(--radius-lg);
  display: inline-flex;
  height: 2rem;
  overflow: hidden;
}

.allocation-editor-header__run-button {
  border-radius: 0;
  height: 2rem;
}

.allocation-editor-header__run-button--main {
  gap: 0.5rem;
  padding-inline: 0.75rem;
  white-space: nowrap;
}

.allocation-editor-header__run-button--trigger {
  border-left: 1px solid color-mix(in srgb, var(--color-emerald-500) 30%, transparent);
  justify-content: center;
  padding-inline: 0.625rem;
}

.allocation-editor-header__menu-check {
  font-size: var(--text-xs);
  margin-left: 0.5rem;
}

:global(.dark .allocation-editor-header) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-800);
}

:global(.dark .allocation-editor-header__eyebrow) {
  color: var(--color-neutral-400);
}

:global(.dark .allocation-editor-header__summary) {
  color: var(--color-neutral-200);
}

:global(.dark .allocation-editor-header__metric-panel) {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-800);
  color: var(--color-neutral-300);
}

:global(.dark .allocation-editor-header__metric-label),
:global(.dark .allocation-editor-header__metric-percent) {
  color: var(--color-neutral-500);
}

:global(.dark .allocation-editor-header__run-button--trigger) {
  border-left-color: color-mix(in srgb, var(--color-emerald-300) 30%, transparent);
}
</style>
