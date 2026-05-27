<template>
  <section class="core-provision-panel">
    <div class="core-provision-panel__header">
      <div class="core-provision-panel__heading">
        <p class="core-provision-panel__eyebrow">Provisioning</p>
        <p class="core-provision-panel__meta">
          Mode: <span class="core-provision-panel__meta-strong">{{ modeText }}</span>
          <template v-if="snapshot?.last_event"> · {{ snapshot.last_event }}</template>
        </p>
        <p class="core-provision-panel__subtle">
          Project root: <code class="core-provision-panel__code">{{ snapshot?.project_root || "—" }}</code>
        </p>
        <p v-if="errorText" class="core-provision-panel__error">{{ errorText }}</p>
      </div>
      <div class="core-provision-panel__actions">
        <button
          type="button"
          class="core-provision-panel__button core-provision-panel__button--secondary"
          :disabled="busy"
          @click="runSmokeCheck"
        >
          Run smoke check
        </button>
      </div>
    </div>

    <div class="core-provision-panel__grid">
      <div class="core-provision-panel__column">
        <section class="core-provision-panel__card">
          <div class="core-provision-panel__card-header">
            <p class="core-provision-panel__card-title">Provision checks</p>
            <span class="core-provision-panel__card-meta">{{ checks.length }} checks</span>
          </div>
          <ChecksList :items="checks" empty-text="No checks reported yet." />
        </section>

        <section class="core-provision-panel__card">
          <div class="core-provision-panel__card-header">
            <p class="core-provision-panel__card-title">Smoke checks</p>
            <span class="core-provision-panel__card-meta">{{ smokeChecks.length }} checks</span>
          </div>
          <ChecksList :items="smokeChecks" empty-text="No smoke-check results yet. Run smoke check." />
        </section>
      </div>

      <div class="core-provision-panel__column">
        <section class="core-provision-panel__card">
          <p class="core-provision-panel__card-title core-provision-panel__card-title--spaced">Host Agent Install / Repair</p>
          <div class="core-provision-panel__button-grid">
            <button
              type="button"
              class="core-provision-panel__button core-provision-panel__button--primary"
              :disabled="busy"
              @click="installNetAgent"
            >
              Install / Repair Core Network Agent
            </button>
            <button
              type="button"
              class="core-provision-panel__button core-provision-panel__button--primary"
              :disabled="busy"
              @click="installNtpAgent"
            >
              Install / Repair NTP Agent
            </button>
            <button
              type="button"
              class="core-provision-panel__button core-provision-panel__button--primary"
              :disabled="busy"
              @click="installDiagAgent"
            >
              Install / Repair Diagnostics Agent
            </button>
          </div>

          <div v-if="snapshot?.request_in_flight" class="core-provision-panel__in-flight">
            In progress: {{ snapshot.request_in_flight.action }} ({{ snapshot.request_in_flight.request_id }})
          </div>
        </section>

        <section class="core-provision-panel__card">
          <p class="core-provision-panel__card-title core-provision-panel__card-title--spaced">Last action result</p>
          <div v-if="lastAction" class="core-provision-panel__last-action">
            <p class="core-provision-panel__last-action-line">
              <span class="core-provision-panel__last-action-name">{{ lastAction.action }}</span>
              ·
              <span :class="lastAction.success ? 'core-provision-panel__result--success' : 'core-provision-panel__result--danger'">
                {{ lastAction.success ? "SUCCESS" : "FAILED" }}
              </span>
              <template v-if="lastAction.duration_ms != null"> · {{ lastAction.duration_ms }} ms</template>
              <template v-if="lastAction.exit_code != null"> · exit {{ lastAction.exit_code }}</template>
            </p>
            <p class="core-provision-panel__last-action-message">{{ lastAction.message }}</p>
          </div>
          <p v-else class="core-provision-panel__empty">
            No provisioning action has been executed yet.
          </p>
        </section>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, defineComponent, h, onMounted, onUnmounted } from "vue"
import { useCoreProvisionStore } from "@/stores/coreProvisionStore"
import type { CoreProvisionCheck } from "@/types/coreProvision"

const store = useCoreProvisionStore()

const snapshot = computed(() => store.snapshot)
const checks = computed(() => store.checks)
const smokeChecks = computed(() => store.smokeChecks)
const lastAction = computed(() => store.lastAction)
const busy = computed(() => store.loading || store.commandPending)
const modeText = computed(() => String(store.mode).toUpperCase())
const errorText = computed(() => store.lastError || snapshot.value?.last_error || null)

async function runSmokeCheck() {
  await store.runSmokeCheck().catch(() => undefined)
}
async function installNetAgent() {
  await store.installNetAgent().catch(() => undefined)
}
async function installNtpAgent() {
  await store.installNtpAgent().catch(() => undefined)
}
async function installDiagAgent() {
  await store.installDiagAgent().catch(() => undefined)
}

onMounted(() => {
  store.startMonitoring()
})
onUnmounted(() => {
  store.stopMonitoring()
})

function statusText(ok: boolean | null): string {
  if (ok === true) return "OK"
  if (ok === false) return "FAIL"
  return "UNKNOWN"
}
function statusPillClass(ok: boolean | null): string {
  if (ok === true) return "core-provision-checks-list__pill--success"
  if (ok === false) return "core-provision-checks-list__pill--danger"
  return "core-provision-checks-list__pill--neutral"
}

const ChecksList = defineComponent({
  name: "CoreProvisionChecksList",
  props: {
    items: { type: Array as () => CoreProvisionCheck[], required: true },
    emptyText: { type: String, required: true },
  },
  setup(props) {
    return () => h("div", { class: "core-provision-checks-list" }, [
      ...(props.items.length
        ? props.items.map((item) =>
            h("div", { class: "core-provision-checks-list__row", key: item.key }, [
              h("div", { class: "core-provision-checks-list__body" }, [
                h("div", { class: "core-provision-checks-list__label" }, item.label),
                item.detail ? h("div", { class: "core-provision-checks-list__detail" }, item.detail) : null,
              ]),
              h("span", { class: `core-provision-checks-list__pill ${statusPillClass(item.ok)}` }, statusText(item.ok)),
            ]),
          )
        : [h("p", { class: "core-provision-checks-list__empty" }, props.emptyText)]),
    ])
  },
})
</script>

<style scoped>
.core-provision-panel {
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
}

.core-provision-panel__header {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
}

.core-provision-panel__heading {
  min-width: 0;
}

.core-provision-panel__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  letter-spacing: 0;
  text-transform: uppercase;
}

.core-provision-panel__meta {
  margin-top: 0.25rem;
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
}

.core-provision-panel__meta-strong {
  font-weight: 600;
  text-transform: uppercase;
}

.core-provision-panel__subtle {
  margin-top: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.core-provision-panel__code {
  padding: 0.125rem 0.25rem;
  border-radius: var(--radius-sm);
  background: var(--color-neutral-100);
}

.core-provision-panel__error {
  margin-top: 0.25rem;
  color: var(--color-rose-500);
  font-size: var(--text-xs);
}

.core-provision-panel__actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
}

.core-provision-panel__grid {
  display: grid;
  gap: 0.75rem;
  margin-top: 0.75rem;
}

.core-provision-panel__column {
  display: grid;
  gap: 0.75rem;
}

.core-provision-panel__card {
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-neutral-50) 80%, transparent);
}

.core-provision-panel__card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
  margin-bottom: 0.5rem;
}

.core-provision-panel__card-title {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  font-weight: 600;
}

.core-provision-panel__card-title--spaced {
  margin-bottom: 0.5rem;
}

.core-provision-panel__card-meta {
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-provision-panel__button-grid {
  display: grid;
  gap: 0.5rem;
}

.core-provision-panel__button {
  border-radius: var(--radius-md);
  font-size: var(--text-xs);
  font-weight: 600;
}

.core-provision-panel__button:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}

.core-provision-panel__button--secondary {
  padding: 0.375rem 0.75rem;
  border: 1px solid var(--color-neutral-300);
  background: transparent;
  color: var(--color-neutral-700);
}

.core-provision-panel__button--secondary:hover:not(:disabled) {
  border-color: var(--color-neutral-500);
}

.core-provision-panel__button--primary {
  padding: 0.5rem 0.75rem;
  border: 0;
  background: var(--color-neutral-900);
  color: var(--color-white);
}

.core-provision-panel__button--primary:hover:not(:disabled) {
  background: var(--color-neutral-700);
}

.core-provision-panel__in-flight {
  margin-top: 0.75rem;
  padding: 0.25rem 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-amber-300) 70%, transparent);
  border-radius: var(--radius-md);
  background: var(--color-amber-50);
  color: var(--color-yellow-800);
  font-size: 11px;
}

.core-provision-panel__last-action {
  display: grid;
  gap: 0.25rem;
  font-size: var(--text-xs);
}

.core-provision-panel__last-action-line {
  color: var(--color-neutral-700);
}

.core-provision-panel__last-action-name {
  font-weight: 600;
}

.core-provision-panel__last-action-message {
  overflow-wrap: anywhere;
  color: var(--color-neutral-500);
  white-space: pre-wrap;
}

.core-provision-panel__result--success {
  color: var(--color-emerald-600);
}

.core-provision-panel__result--danger {
  color: var(--color-rose-600);
}

.core-provision-panel__empty {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

:global(.core-provision-checks-list) {
  max-height: 14rem;
  padding: 0.25rem;
  overflow-y: auto;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

:global(.core-provision-checks-list__row) {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.5rem;
  padding: 0.25rem 0.5rem;
  font-size: var(--text-xs);
}

:global(.core-provision-checks-list__body) {
  min-width: 0;
}

:global(.core-provision-checks-list__label) {
  overflow: hidden;
  color: var(--color-neutral-800);
  text-overflow: ellipsis;
  white-space: nowrap;
}

:global(.core-provision-checks-list__detail) {
  overflow: hidden;
  color: var(--color-neutral-500);
  font-size: 11px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

:global(.core-provision-checks-list__pill) {
  flex-shrink: 0;
  padding: 0.125rem 0.375rem;
  border-radius: var(--radius-sm);
  font-size: 10px;
  font-weight: 600;
}

:global(.core-provision-checks-list__pill--success) {
  background: var(--color-green-100);
  color: var(--color-green-800);
}

:global(.core-provision-checks-list__pill--danger) {
  background: var(--color-red-100);
  color: var(--color-red-800);
}

:global(.core-provision-checks-list__pill--neutral) {
  background: var(--color-neutral-200);
  color: var(--color-neutral-700);
}

:global(.core-provision-checks-list__empty) {
  padding: 0.5rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

@media (min-width: 1280px) {
  .core-provision-panel__grid {
    grid-template-columns: minmax(0, 1.15fr) minmax(0, 0.85fr);
  }
}

:global(.dark .core-provision-panel){
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
}

:global(.dark .core-provision-panel__meta),
:global(.dark .core-provision-panel__card-title),
:global(.dark .core-provision-panel__last-action-line){
  color: var(--color-neutral-200);
}

:global(.dark .core-provision-panel__eyebrow),
:global(.dark .core-provision-panel__subtle),
:global(.dark .core-provision-panel__card-meta),
:global(.dark .core-provision-panel__last-action-message),
:global(.dark .core-provision-panel__empty){
  color: var(--color-neutral-400);
}

:global(.dark .core-provision-panel__code){
  background: var(--color-neutral-800);
}

:global(.dark .core-provision-panel__card){
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 60%, transparent);
}

:global(.dark .core-provision-panel__button--secondary){
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-200);
}

:global(.dark .core-provision-panel__button--secondary:hover:not(:disabled)){
  border-color: var(--color-neutral-500);
}

:global(.dark .core-provision-panel__button--primary){
  background: var(--color-neutral-100);
  color: var(--color-neutral-900);
}

:global(.dark .core-provision-panel__button--primary:hover:not(:disabled)){
  background: var(--color-neutral-300);
}

:global(.dark .core-provision-panel__in-flight){
  border-color: color-mix(in srgb, var(--color-amber-700) 50%, transparent);
  background: color-mix(in srgb, var(--color-amber-900) 40%, transparent);
  color: var(--color-amber-300);
}

:global(.dark .core-provision-panel__result--success){
  color: var(--color-emerald-300);
}

:global(.dark .core-provision-panel__result--danger){
  color: var(--color-rose-300);
}

:global(.dark .core-provision-checks-list) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 50%, transparent);
}

:global(.dark .core-provision-checks-list__label) {
  color: var(--color-neutral-100);
}

:global(.dark .core-provision-checks-list__detail),
:global(.dark .core-provision-checks-list__empty) {
  color: var(--color-neutral-400);
}

:global(.dark .core-provision-checks-list__pill--success) {
  background: color-mix(in srgb, var(--color-emerald-900) 40%, transparent);
  color: var(--color-emerald-300);
}

:global(.dark .core-provision-checks-list__pill--danger) {
  background: color-mix(in srgb, var(--color-red-900) 40%, transparent);
  color: var(--color-red-300);
}

:global(.dark .core-provision-checks-list__pill--neutral) {
  background: var(--color-neutral-800);
  color: var(--color-neutral-200);
}
</style>
