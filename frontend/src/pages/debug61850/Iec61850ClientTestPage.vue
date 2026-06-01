<script setup lang="ts">
import { computed, onMounted, ref } from "vue"
import { RouterLink } from "vue-router"

import UiButton from "@/components/ui/UiButton.vue"
import { normalizeHttpError } from "@/api/http"
import { Iec61850ClientAPI, type Iec61850ClientState } from "@/api/iec61850Client.api"

const state = ref<Iec61850ClientState | null>(null)
const loading = ref(false)
const busyAction = ref<string | null>(null)
const errorMessage = ref<string | null>(null)

const transcript = computed(() => state.value?.transcript ?? [])
const lastDiagnostic = computed(() => state.value?.last_diagnostic ?? null)
const sessionStatus = computed(() => state.value?.session_open ? "Connected" : "Disconnected")
const liveWireStatus = computed(() => state.value?.live_wire_open ? "Wire connected" : "Wire closed")
const reportStatus = computed(() => state.value?.last_report ? "Report received" : "Waiting for report")
const wireFrameStatus = computed(() => state.value?.live_wire_last_frame_length ? `${state.value.live_wire_last_frame_length} bytes` : "No frame yet")
const stateSummary = computed(() => {
  if (!state.value) return "No client session loaded"
  return `${state.value.endpoint.ied_name}/${state.value.endpoint.access_point_name} · ${state.value.candidate.report_control_name}`
})

onMounted(async () => {
  await refreshState()
})

async function refreshState() {
  loading.value = true
  errorMessage.value = null
  try {
    state.value = await Iec61850ClientAPI.state()
  } catch (error) {
    errorMessage.value = normalizeHttpError(error).message
  } finally {
    loading.value = false
  }
}

async function runAction(action: string, operation: () => Promise<Iec61850ClientState>) {
  busyAction.value = action
  errorMessage.value = null
  try {
    state.value = await operation()
  } catch (error) {
    errorMessage.value = normalizeHttpError(error).message
    await refreshState()
  } finally {
    busyAction.value = null
  }
}

function formatJson(value: unknown): string {
  return JSON.stringify(value, null, 2)
}
</script>

<template>
  <div class="iec61850-client-page">
    <header class="iec61850-client-page__header">
      <div class="iec61850-client-page__title-block">
        <p class="iec61850-client-page__eyebrow">IEC 61850</p>
        <h1 class="iec61850-client-page__title">Client Test Page</h1>
        <p class="iec61850-client-page__status">{{ stateSummary }}</p>
      </div>
      <div class="iec61850-client-page__actions">
        <RouterLink class="iec61850-client-page__nav-link iec61850-client-page__action" to="/61850-debug">
          Back to Debug
        </RouterLink>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="loading" @click="refreshState">
          Refresh state
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('open', Iec61850ClientAPI.openSession)">
          {{ busyAction === 'open' ? 'Opening...' : 'Open session' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('read', Iec61850ClientAPI.readReportControl)">
          {{ busyAction === 'read' ? 'Reading...' : 'Read RCB' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('reserve', Iec61850ClientAPI.reserveReportControl)">
          {{ busyAction === 'reserve' ? 'Reserving...' : 'Reserve' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('enable', Iec61850ClientAPI.enableReportControl)">
          {{ busyAction === 'enable' ? 'Enabling...' : 'Enable' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('gi', Iec61850ClientAPI.sendGeneralInterrogation)">
          {{ busyAction === 'gi' ? 'Requesting GI...' : 'Request GI' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('disable', Iec61850ClientAPI.disableReportControl)">
          {{ busyAction === 'disable' ? 'Disabling...' : 'Disable' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('release', Iec61850ClientAPI.releaseReportControl)">
          {{ busyAction === 'release' ? 'Releasing...' : 'Release' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('subscription', Iec61850ClientAPI.runSubscriptionPlan)">
          {{ busyAction === 'subscription' ? 'Running...' : 'Run subscription' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('wire-start', Iec61850ClientAPI.startWireTransport)">
          {{ busyAction === 'wire-start' ? 'Starting wire...' : 'Start wire' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('wire-emit', Iec61850ClientAPI.emitWireReport)">
          {{ busyAction === 'wire-emit' ? 'Emitting report...' : 'Emit wire report' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('wire-stop', Iec61850ClientAPI.stopWireTransport)">
          {{ busyAction === 'wire-stop' ? 'Stopping wire...' : 'Stop wire transport' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('close', Iec61850ClientAPI.closeSession)">
          {{ busyAction === 'close' ? 'Closing...' : 'Close session' }}
        </UiButton>
        <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('clear', Iec61850ClientAPI.clearTranscript)">
          {{ busyAction === 'clear' ? 'Clearing...' : 'Clear transcript' }}
        </UiButton>
      </div>
    </header>

    <section v-if="errorMessage" class="iec61850-client-page__alert">
      {{ errorMessage }}
    </section>

    <section class="iec61850-client-page__summary">
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Session</span>
        <span class="iec61850-client-page__metric-value">{{ sessionStatus }}</span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Transcript</span>
        <span class="iec61850-client-page__metric-value">{{ transcript.length }}</span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Last report</span>
        <span class="iec61850-client-page__metric-value">{{ reportStatus }}</span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Live wire</span>
        <span class="iec61850-client-page__metric-value">{{ liveWireStatus }}</span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Last wire frame</span>
        <span class="iec61850-client-page__metric-value">{{ wireFrameStatus }}</span>
      </div>
      <div class="iec61850-client-page__metric iec61850-client-page__metric--wide">
        <span class="iec61850-client-page__metric-label">Last diagnostic</span>
        <span class="iec61850-client-page__metric-value">{{ lastDiagnostic ? `${lastDiagnostic.action}: ${lastDiagnostic.code}` : 'None' }}</span>
      </div>
    </section>

    <main class="iec61850-client-page__workspace">
      <section class="iec61850-client-page__panel iec61850-client-page__panel--state">
        <div class="iec61850-client-page__panel-header">
          <h2>Current state</h2>
        </div>
        <div class="iec61850-client-page__panel-body">
          <pre class="iec61850-client-page__json">{{ state ? formatJson(state) : 'No state loaded' }}</pre>
        </div>
      </section>

      <section class="iec61850-client-page__panel iec61850-client-page__panel--transcript">
        <div class="iec61850-client-page__panel-header">
          <h2>Transcript</h2>
        </div>
        <div class="iec61850-client-page__panel-body iec61850-client-page__panel-body--scroll">
          <div v-if="state?.live_wire_last_frame_hex" class="iec61850-client-page__wire-frame">
            <p class="iec61850-client-page__wire-frame-label">Last wire frame hex</p>
            <pre class="iec61850-client-page__json">{{ state.live_wire_last_frame_hex }}</pre>
          </div>
          <div v-if="transcript.length" class="iec61850-client-page__transcript-list">
            <article v-for="event in transcript" :key="event.id" class="iec61850-client-page__transcript-item">
              <div class="iec61850-client-page__transcript-head">
                <strong>{{ event.kind }}</strong>
                <span>{{ event.at }}</span>
              </div>
              <div class="iec61850-client-page__transcript-meta">
                <span>{{ event.session_id }}</span>
                <span>{{ event.endpoint_id }}</span>
                <span v-if="event.client_id">client {{ event.client_id }}</span>
                <span v-if="event.outcome">outcome {{ event.outcome }}</span>
                <span v-if="event.code">{{ event.code }}</span>
              </div>
              <p v-if="event.message" class="iec61850-client-page__transcript-message">{{ event.message }}</p>
            </article>
          </div>
          <p v-else class="iec61850-client-page__empty">No transcript events yet.</p>
        </div>
      </section>
    </main>
  </div>
</template>

<style scoped>

.iec61850-client-page {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  height: 100%;
  min-height: 0;
  padding: 0.75rem;
  overflow: hidden;
  color: var(--color-neutral-900);
}

.iec61850-client-page__header,
.iec61850-client-page__summary,
.iec61850-client-page__panel {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: 16px;
  box-shadow: 0 10px 24px rgba(15, 23, 42, 0.05);
}

.iec61850-client-page__header {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.75rem 0.9rem;
}

.iec61850-client-page__title-block {
  display: grid;
  gap: 0.2rem;
  min-width: 0;
}

.iec61850-client-page__eyebrow,
.iec61850-client-page__status,
.iec61850-client-page__panel-header {
  text-transform: uppercase;
  letter-spacing: 0.12em;
  font-size: 0.68rem;
}

.iec61850-client-page__eyebrow,
.iec61850-client-page__status {
  margin: 0;
  color: var(--color-neutral-500);
}

.iec61850-client-page__title {
  margin: 0;
  font-size: clamp(1.35rem, 2.2vw, 1.85rem);
  line-height: 1.1;
}

.iec61850-client-page__actions {
  display: flex;
  flex-wrap: wrap;
  gap: 0.35rem;
  justify-content: flex-end;
  max-width: min(100%, 68rem);
}

.iec61850-client-page__nav-link {
  text-decoration: none;
}

.iec61850-client-page__action {
  flex: 0 0 auto;
  white-space: nowrap;
}

.iec61850-client-page__nav-link:hover {
  text-decoration: none;
}

.iec61850-client-page__alert {
  border-radius: 14px;
  padding: 0.9rem 1rem;
  border: 1px solid var(--color-red-200);
  background: var(--color-red-50);
  color: var(--color-red-800);
}

.iec61850-client-page__summary {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 0.5rem;
  padding: 0.75rem;
}

.iec61850-client-page__metric {
  display: grid;
  gap: 0.15rem;
  padding: 0.7rem 0.8rem;
  border-radius: 12px;
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  min-width: 0;
}

.iec61850-client-page__metric--wide {
  grid-column: 1 / -1;
}

.iec61850-client-page__metric-label {
  color: var(--color-neutral-500);
  font-size: 0.68rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.iec61850-client-page__wire-frame {
  display: grid;
  gap: 0.75rem;
  margin-bottom: 1rem;
}

.iec61850-client-page__wire-frame-label {
  margin: 0;
  font-size: 0.72rem;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  color: var(--color-neutral-500);
}

.iec61850-client-page__metric-value {
  font-size: 0.9rem;
  font-weight: 600;
  overflow-wrap: anywhere;
}

.iec61850-client-page__workspace {
  display: grid;
  grid-template-columns: minmax(0, 1.15fr) minmax(0, 1fr);
  grid-template-rows: minmax(0, 1fr);
  gap: 1rem;
  flex: 1 1 auto;
  min-height: 0;
}

.iec61850-client-page__panel {
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
  padding: 0.9rem;
}

.iec61850-client-page__panel-body {
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
}

.iec61850-client-page__panel-body--scroll {
  flex: 1 1 auto;
  overflow: auto;
}

.iec61850-client-page__panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: var(--color-neutral-500);
  margin-bottom: 0.5rem;
}

.iec61850-client-page__panel-header h2 {
  margin: 0;
  font-size: 0.78rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.iec61850-client-page__panel--state .iec61850-client-page__json,
.iec61850-client-page__panel--transcript .iec61850-client-page__json {
  min-height: 0;
  overflow: auto;
}

.iec61850-client-page__panel--transcript {
  overflow: hidden;
}

.iec61850-client-page__panel--transcript .iec61850-client-page__wire-frame {
  flex: 0 0 auto;
}

.iec61850-client-page__panel--transcript .iec61850-client-page__transcript-list {
  flex: 0 0 auto;
  min-height: 0;
}

.iec61850-client-page__json {
  margin: 0;
  padding: 0.85rem;
  border-radius: 12px;
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  color: var(--color-neutral-900);
  font-size: 0.8rem;
  line-height: 1.45;
}

.iec61850-client-page__transcript-list {
  display: flex;
  flex-direction: column;
  gap: 0.55rem;
  min-height: 0;
}

.iec61850-client-page__transcript-item {
  min-width: 0;
  max-width: 100%;
  padding: 0.7rem 0.8rem;
  border-radius: 12px;
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  overflow: hidden;
  overflow-wrap: anywhere;
  word-break: break-word;
  flex: 0 0 auto;
}

.iec61850-client-page__transcript-head,
.iec61850-client-page__transcript-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 0.4rem 0.65rem;
  min-width: 0;
  max-width: 100%;
}

.iec61850-client-page__transcript-head {
  justify-content: space-between;
  align-items: center;
  margin-bottom: 0.35rem;
}

.iec61850-client-page__transcript-head span,
.iec61850-client-page__transcript-meta,
.iec61850-client-page__transcript-message,
.iec61850-client-page__empty {
  color: var(--color-neutral-700);
  font-size: 0.8rem;
  min-width: 0;
  max-width: 100%;
  overflow-wrap: anywhere;
  word-break: break-word;
}

.iec61850-client-page__transcript-message,
.iec61850-client-page__empty {
  margin: 0.4rem 0 0;
}

.iec61850-client-page__transcript-head strong,
.iec61850-client-page__transcript-head span {
  min-width: 0;
  max-width: 100%;
  overflow-wrap: anywhere;
  word-break: break-word;
}

@media (max-width: 960px) {
  .iec61850-client-page {
    height: auto;
    overflow: visible;
  }

  .iec61850-client-page__workspace {
    grid-template-columns: 1fr;
    flex: 0 0 auto;
  }

  .iec61850-client-page__panel--state .iec61850-client-page__json,
  .iec61850-client-page__panel--transcript .iec61850-client-page__json,
  .iec61850-client-page__panel--transcript .iec61850-client-page__transcript-list,
  .iec61850-client-page__panel-body--scroll {
    overflow: visible;
  }
}

:global(.dark .iec61850-client-page) {
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-client-page__header),
:global(.dark .iec61850-client-page__summary),
:global(.dark .iec61850-client-page__panel) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-800);
  box-shadow: 0 18px 40px rgba(2, 6, 23, 0.28);
}

:global(.dark .iec61850-client-page__eyebrow),
:global(.dark .iec61850-client-page__status),
:global(.dark .iec61850-client-page__panel-header),
:global(.dark .iec61850-client-page__metric-label),
:global(.dark .iec61850-client-page__wire-frame-label) {
  color: var(--color-neutral-400);
}

:global(.dark .iec61850-client-page__nav-link) {
  background: var(--color-neutral-800);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-client-page__nav-link:hover) {
  background: var(--color-neutral-700);
}

:global(.dark .iec61850-client-page__alert) {
  border-color: rgba(248, 113, 113, 0.35);
  background: rgba(127, 29, 29, 0.28);
  color: #fecaca;
}

:global(.dark .iec61850-client-page__metric) {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-800);
}

:global(.dark .iec61850-client-page__json),
:global(.dark .iec61850-client-page__transcript-item) {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-client-page__transcript-head span),
:global(.dark .iec61850-client-page__transcript-meta),
:global(.dark .iec61850-client-page__transcript-message),
:global(.dark .iec61850-client-page__empty) {
  color: var(--color-neutral-300);
}

</style>
