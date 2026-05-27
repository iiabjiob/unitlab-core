<template>
  <header class="allocation-editor-header">
    <div class="allocation-editor-header__stack">
      <div class="allocation-editor-header__copy">
        <p class="allocation-editor-header__eyebrow">Live Signal Sheet</p>
        <p class="allocation-editor-header__summary">
          {{ summaryText }}
        </p>
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

<style scoped>
.allocation-editor-header {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
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
  background: color-mix(in srgb, var(--color-emerald-500) 10%, transparent);
  border: 1px solid color-mix(in srgb, var(--color-emerald-500) 30%, transparent);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow-sm), 0 1px 2px color-mix(in srgb, var(--color-emerald-500) 25%, transparent);
  display: inline-flex;
  height: 2rem;
  overflow: hidden;
}

.allocation-editor-header__run-button {
  align-items: center;
  border-radius: 0;
  display: flex;
  font-size: var(--text-sm);
  font-weight: 600;
  height: 2rem;
  letter-spacing: 0;
  line-height: 1;
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

:global(.dark .allocation-editor-header__run-segment) {
  background: color-mix(in srgb, var(--color-emerald-400) 10%, transparent);
  border-color: color-mix(in srgb, var(--color-emerald-400) 40%, transparent);
}

:global(.dark .allocation-editor-header__run-button--trigger) {
  border-left-color: color-mix(in srgb, var(--color-emerald-300) 30%, transparent);
}
</style>
