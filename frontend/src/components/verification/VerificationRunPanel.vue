<template>
  <section class="verification-run-panel">
    <div class="verification-run-panel__header">
      <div class="verification-run-panel__copy">
        <p class="verification-run-panel__eyebrow">Verification</p>
        <p class="verification-run-panel__meta">
          One or more allocated signals across selected IEDs. Automatic discovery. Persisted evidence.
        </p>
        <p v-if="selectedSignalLabel" class="verification-run-panel__subtle">
          Selected: <span class="verification-run-panel__subtle-strong">{{ selectedSignalLabel }}</span>
        </p>
        <p v-if="errorText" class="verification-run-panel__error">
          {{ errorText }}
        </p>
        <div v-if="networkPreflightView" class="verification-run-panel__network" :class="`verification-run-panel__network--${networkPreflightView.stateTone}`">
          <div class="verification-run-panel__network-headline">{{ networkPreflightView.headline }}</div>
          <div class="verification-run-panel__network-summary">{{ networkPreflightView.summary }}</div>
          <div class="verification-run-panel__network-runtime">{{ networkPreflightView.runtimeSummary }}</div>
          <ul v-if="networkPreflightView.groupHints.length > 0" class="verification-run-panel__network-list">
            <li v-for="item in networkPreflightView.groupHints" :key="item" class="verification-run-panel__network-item">
              {{ item }}
            </li>
          </ul>
        </div>
      </div>
      <div class="verification-run-panel__actions">
        <UiButton
          variant="success"
          size="sm"
          :disabled="busy || !canRun"
          @click="emit('run')"
        >
          {{ busy ? "Verifying..." : "Run verification" }}
        </UiButton>
      </div>
    </div>

    <div v-if="viewModel" class="verification-run-panel__result">
      <div class="verification-run-panel__verdict" :class="verdictToneClass">
        <div class="verification-run-panel__verdict-headline">{{ viewModel.headline }}</div>
        <div class="verification-run-panel__verdict-summary">{{ viewModel.summary }}</div>
      </div>

      <div class="verification-run-panel__facts">
        <div class="verification-run-panel__fact">
          <span class="verification-run-panel__fact-label">Run</span>
          <span class="verification-run-panel__fact-value">{{ viewModel.testRunId }}</span>
        </div>
        <div class="verification-run-panel__fact">
          <span class="verification-run-panel__fact-label">Verdict</span>
          <span class="verification-run-panel__fact-value">{{ viewModel.verdictState }}</span>
        </div>
        <div class="verification-run-panel__fact">
          <span class="verification-run-panel__fact-label">Confidence</span>
          <span class="verification-run-panel__fact-value">{{ viewModel.verificationConfidence }}</span>
        </div>
        <div class="verification-run-panel__fact">
          <span class="verification-run-panel__fact-label">Evidence</span>
          <span class="verification-run-panel__fact-value">{{ viewModel.evidenceCount }}</span>
        </div>
        <div class="verification-run-panel__fact verification-run-panel__fact--wide">
          <span class="verification-run-panel__fact-label">Confidence reason</span>
          <span class="verification-run-panel__fact-value">{{ viewModel.confidenceReason }}</span>
        </div>
      </div>

      <div class="verification-run-panel__signals">
        <article v-for="signal in viewModel.signals" :key="`${viewModel.testRunId}:${signal.signalId}`" class="verification-run-panel__signal-card">
          <div class="verification-run-panel__signal-title">{{ signal.title }}</div>
          <div class="verification-run-panel__signal-grid">
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Output</span>
              <span class="verification-run-panel__signal-value">{{ signal.output }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Expected</span>
              <span class="verification-run-panel__signal-value">{{ signal.expected }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Observed</span>
              <span class="verification-run-panel__signal-value">{{ signal.observed }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Unit</span>
              <span class="verification-run-panel__signal-value">{{ signal.unit }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Endpoint</span>
              <span class="verification-run-panel__signal-value">{{ signal.endpoint }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">RCB</span>
              <span class="verification-run-panel__signal-value">{{ signal.rcb }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Dataset</span>
              <span class="verification-run-panel__signal-value">{{ signal.dataset }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Latency</span>
              <span class="verification-run-panel__signal-value">{{ signal.latency }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Why</span>
              <span class="verification-run-panel__signal-value">{{ signal.reason }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Confidence</span>
              <span class="verification-run-panel__signal-value">{{ signal.verificationConfidence }}</span>
            </div>
            <div class="verification-run-panel__signal-row">
              <span class="verification-run-panel__signal-label">Confidence reason</span>
              <span class="verification-run-panel__signal-value">{{ signal.confidenceReason }}</span>
            </div>
          </div>
          <div class="verification-run-panel__signal-badges">
            <span class="verification-run-panel__badge">{{ signal.evidenceStatus }}</span>
            <span class="verification-run-panel__badge">{{ signal.verdictState }}</span>
          </div>
        </article>
      </div>

      <div v-if="viewModel.diagnostics.length > 0" class="verification-run-panel__diagnostics">
        <p class="verification-run-panel__section-title">Diagnostics</p>
        <ul class="verification-run-panel__diagnostics-list">
          <li v-for="item in viewModel.diagnostics" :key="item" class="verification-run-panel__diagnostics-item">
            {{ item }}
          </li>
        </ul>
      </div>
    </div>

    <p v-else class="verification-run-panel__empty">
      Select one or more allocated rows, then run automatic verification.
    </p>
  </section>
</template>

<script setup lang="ts">
import { computed } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import { buildVerificationNetworkPreflightView } from "@/components/verification/verificationNetworkPreflightView"
import { buildVerificationRunView } from "@/components/verification/verificationRunView"
import type { VerificationNetworkPreflightResponse, VerificationRunDetailResponse } from "@/types/verification"

const props = defineProps<{
  busy: boolean
  canRun: boolean
  errorText: string | null
  networkPreflight: VerificationNetworkPreflightResponse | null
  selectedSignalLabel: string | null
  result: VerificationRunDetailResponse | null
}>()

const emit = defineEmits<{
  (event: "run"): void
}>()

const viewModel = computed(() => buildVerificationRunView(props.result))
const networkPreflightView = computed(() => buildVerificationNetworkPreflightView(props.networkPreflight))
const verdictToneClass = computed(() => {
  const verdict = viewModel.value?.verdictState ?? ""
  if (verdict === "PASS") return "verification-run-panel__verdict--pass"
  if (verdict === "FAIL") return "verification-run-panel__verdict--fail"
  return "verification-run-panel__verdict--neutral"
})
</script>

<style scoped>
.verification-run-panel {
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 94%, var(--color-emerald-100)), var(--color-white));
  border: 1px solid color-mix(in srgb, var(--color-emerald-500) 18%, var(--color-neutral-200));
  border-radius: var(--radius-2xl);
  padding: 1rem;
}

.verification-run-panel__header {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
}

.verification-run-panel__copy {
  min-width: 0;
}

.verification-run-panel__eyebrow {
  color: var(--color-neutral-500);
  font-size: var(--text-compact);
  letter-spacing: 0.16em;
  text-transform: uppercase;
}

.verification-run-panel__meta {
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
  margin-top: 0.25rem;
}

.verification-run-panel__subtle {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  margin-top: 0.25rem;
}

.verification-run-panel__subtle-strong {
  color: var(--color-neutral-700);
  font-weight: 600;
}

.verification-run-panel__error {
  color: var(--color-red-700);
  font-size: var(--text-xs);
  margin-top: 0.5rem;
}

.verification-run-panel__network {
  border-radius: var(--radius-wide);
  margin-top: 0.75rem;
  padding: 0.75rem 0.875rem;
}

.verification-run-panel__network--ready {
  background: color-mix(in srgb, var(--color-emerald-50) 82%, white);
  border: 1px solid color-mix(in srgb, var(--color-emerald-500) 24%, transparent);
}

.verification-run-panel__network--attention_required {
  background: color-mix(in srgb, var(--color-amber-50) 86%, white);
  border: 1px solid color-mix(in srgb, var(--color-amber-500) 28%, transparent);
}

.verification-run-panel__network--unknown {
  background: color-mix(in srgb, var(--color-neutral-50) 94%, white);
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 24%, transparent);
}

.verification-run-panel__network-headline {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 700;
}

.verification-run-panel__network-summary,
.verification-run-panel__network-runtime {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  margin-top: 0.25rem;
}

.verification-run-panel__network-list {
  margin: 0.5rem 0 0;
  padding-left: 1.1rem;
}

.verification-run-panel__network-item {
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

.verification-run-panel__actions {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.verification-run-panel__result {
  display: grid;
  gap: 0.875rem;
  margin-top: 1rem;
}

.verification-run-panel__verdict {
  border-radius: var(--radius-wide);
  padding: 0.875rem 1rem;
}

.verification-run-panel__verdict--pass {
  background: color-mix(in srgb, var(--color-emerald-50) 88%, white);
  border: 1px solid color-mix(in srgb, var(--color-emerald-500) 24%, transparent);
}

.verification-run-panel__verdict--fail {
  background: color-mix(in srgb, var(--color-red-50) 88%, white);
  border: 1px solid color-mix(in srgb, var(--color-red-500) 24%, transparent);
}

.verification-run-panel__verdict--neutral {
  background: color-mix(in srgb, var(--color-neutral-50) 94%, white);
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 24%, transparent);
}

.verification-run-panel__verdict-headline {
  color: var(--color-neutral-900);
  font-size: var(--text-lg);
  font-weight: 700;
  letter-spacing: 0.06em;
  text-transform: uppercase;
}

.verification-run-panel__verdict-summary {
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
  margin-top: 0.25rem;
}

.verification-run-panel__facts {
  display: grid;
  gap: 0.5rem;
  grid-template-columns: repeat(auto-fit, minmax(8rem, 1fr));
}

.verification-run-panel__fact {
  background: color-mix(in srgb, var(--color-white) 92%, transparent);
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: var(--radius-xl);
  padding: 0.625rem 0.75rem;
}

.verification-run-panel__fact-label {
  color: var(--color-neutral-500);
  display: block;
  font-size: var(--text-compact);
  letter-spacing: 0.12em;
  text-transform: uppercase;
}

.verification-run-panel__fact-value {
  color: var(--color-neutral-900);
  display: block;
  font-size: var(--text-sm);
  font-weight: 600;
  margin-top: 0.25rem;
  overflow-wrap: anywhere;
}

.verification-run-panel__fact--wide {
  grid-column: 1 / -1;
}

.verification-run-panel__signals {
  display: grid;
  gap: 0.75rem;
}

.verification-run-panel__signal-card {
  background: var(--color-white);
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 78%, transparent);
  border-radius: var(--radius-wide);
  padding: 0.875rem 1rem;
}

.verification-run-panel__signal-title {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 700;
}

.verification-run-panel__signal-grid {
  display: grid;
  gap: 0.5rem;
  grid-template-columns: repeat(auto-fit, minmax(11rem, 1fr));
  margin-top: 0.75rem;
}

.verification-run-panel__signal-row {
  display: grid;
  gap: 0.125rem;
}

.verification-run-panel__signal-label {
  color: var(--color-neutral-500);
  font-size: var(--text-compact);
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.verification-run-panel__signal-value {
  color: var(--color-neutral-800);
  font-size: var(--text-xs);
  overflow-wrap: anywhere;
}

.verification-run-panel__signal-badges {
  display: flex;
  flex-wrap: wrap;
  gap: 0.375rem;
  margin-top: 0.75rem;
}

.verification-run-panel__badge {
  background: color-mix(in srgb, var(--color-neutral-100) 84%, white);
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 72%, transparent);
  border-radius: var(--radius-pill);
  color: var(--color-neutral-700);
  font-size: var(--text-compact);
  font-weight: 600;
  letter-spacing: 0.08em;
  padding: 0.25rem 0.5rem;
  text-transform: uppercase;
}

.verification-run-panel__diagnostics {
  background: color-mix(in srgb, var(--color-neutral-50) 90%, white);
  border-radius: var(--radius-wide);
  padding: 0.875rem 1rem;
}

.verification-run-panel__section-title {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  font-weight: 700;
  letter-spacing: 0.12em;
  margin-bottom: 0.5rem;
  text-transform: uppercase;
}

.verification-run-panel__diagnostics-list {
  display: grid;
  gap: 0.25rem;
  margin: 0;
  padding-left: 1rem;
}

.verification-run-panel__diagnostics-item {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
}

.verification-run-panel__empty {
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
  margin-top: 0.875rem;
}

:global(.dark .verification-run-panel) {
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 92%, var(--color-emerald-950)), var(--color-neutral-900));
  border-color: color-mix(in srgb, var(--color-emerald-400) 18%, var(--color-neutral-800));
}

:global(.dark .verification-run-panel__meta),
:global(.dark .verification-run-panel__subtle),
:global(.dark .verification-run-panel__empty) {
  color: var(--color-neutral-300);
}

:global(.dark .verification-run-panel__subtle-strong),
:global(.dark .verification-run-panel__verdict-headline),
:global(.dark .verification-run-panel__fact-value),
:global(.dark .verification-run-panel__signal-title) {
  color: var(--color-neutral-50);
}

:global(.dark .verification-run-panel__verdict-summary),
:global(.dark .verification-run-panel__signal-value),
:global(.dark .verification-run-panel__diagnostics-item) {
  color: var(--color-neutral-300);
}

:global(.dark .verification-run-panel__fact),
:global(.dark .verification-run-panel__signal-card),
:global(.dark .verification-run-panel__diagnostics) {
  background: color-mix(in srgb, var(--color-neutral-900) 88%, transparent);
  border-color: color-mix(in srgb, var(--color-neutral-700) 72%, transparent);
}

:global(.dark .verification-run-panel__badge) {
  background: color-mix(in srgb, var(--color-neutral-800) 84%, transparent);
  border-color: color-mix(in srgb, var(--color-neutral-600) 72%, transparent);
  color: var(--color-neutral-200);
}

:global(.dark .verification-run-panel__verdict--pass) {
  background: color-mix(in srgb, var(--color-emerald-950) 70%, var(--color-neutral-900));
}

:global(.dark .verification-run-panel__verdict--fail) {
  background: color-mix(in srgb, var(--color-red-950) 70%, var(--color-neutral-900));
}
</style>
