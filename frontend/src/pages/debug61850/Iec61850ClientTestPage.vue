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
const targetHost = ref("host.docker.internal")
const targetPort = ref(12447)
const targetSclPath = ref("/workspace/.refs/sld-rev2.scd")
const targetIedName = ref("KINTE13LVC01")

const transcript = computed(() => state.value?.transcript ?? [])
const uiState = computed(() => state.value?.ui_state ?? null)
const lastDiagnostic = computed(() => uiState.value?.diagnostic.active ? uiState.value.diagnostic : state.value?.last_diagnostic ?? null)
const sessionStatus = computed(() => uiState.value?.session.phase ?? "not-loaded")
const discoveryStatus = computed(() => uiState.value?.discovery.discovered ? "Structure loaded" : "Not discovered")
const liveWireStatus = computed(() => uiState.value?.wire.open ? "Wire connected" : "Wire closed")
const isExternalTarget = computed(() => state.value?.endpoint.mode === "mms")
const externalActionDisabled = computed(() => busyAction.value !== null)
const wireStartDisabled = computed(() => busyAction.value !== null || isExternalTarget.value)
const reportStatus = computed(() => uiState.value?.report.received ? "Report received" : "Waiting for report")
const wireFrameStatus = computed(() => uiState.value?.wire.last_frame_length ? `${uiState.value.wire.last_frame_length} bytes` : "No frame yet")
const selectedReportStatus = computed(() => uiState.value?.discovery.selected_rcb_ref ?? state.value?.candidate.report_control_name ?? "None")
const selectedDatasetStatus = computed(() => uiState.value?.discovery.selected_dataset_ref ?? state.value?.candidate.data_set_ref ?? "None")
const subscriptionCommandStatus = computed(() => {
  const subscription = uiState.value?.subscription
  if (!subscription?.last_command) return "None"
  return subscription.command_accepted ? `${subscription.last_command} accepted` : subscription.last_command
})
const reportValues = computed(() => uiState.value?.report.values ?? [])
const stateSummary = computed(() => {
  if (!state.value) return "No client session loaded"
  return `${state.value.endpoint.ied_name}/${state.value.endpoint.access_point_name} · ${state.value.candidate.report_control_name}`
})
const phaseClass = computed(() => {
  const phase = uiState.value?.session.phase ?? "idle"
  return `iec61850-client-page__phase--${phase}`
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

async function configureExternalTarget() {
  await runAction("target-external", () => Iec61850ClientAPI.configureTarget({
    mode: "external-mms",
    host: targetHost.value.trim(),
    port: Number(targetPort.value),
    scl_path: targetSclPath.value.trim(),
    ied_name: targetIedName.value.trim(),
    access_point_name: "AP1",
  }))
}

async function configureSimulatorTarget() {
  await runAction("target-simulator", () => Iec61850ClientAPI.configureTarget({ mode: "simulator" }))
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
        <div class="iec61850-client-page__action-group">
          <RouterLink class="iec61850-client-page__nav-link iec61850-client-page__action" to="/61850-debug">
            Back to Debug
          </RouterLink>
        </div>
        <div class="iec61850-client-page__target-group">
          <input v-model="targetHost" class="iec61850-client-page__target-input" aria-label="MMS host" placeholder="host" />
          <input v-model.number="targetPort" class="iec61850-client-page__target-input iec61850-client-page__target-input--port" aria-label="MMS port" type="number" min="1" max="65535" />
          <input v-model="targetIedName" class="iec61850-client-page__target-input" aria-label="IED name" placeholder="IED" />
          <input v-model="targetSclPath" class="iec61850-client-page__target-input iec61850-client-page__target-input--path" aria-label="SCD path" placeholder="SCD path" />
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="configureExternalTarget">
            {{ busyAction === 'target-external' ? 'Setting target...' : 'Use external MMS' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="configureSimulatorTarget">
            {{ busyAction === 'target-simulator' ? 'Resetting...' : 'Use fixture sim' }}
          </UiButton>
        </div>
        <div class="iec61850-client-page__action-group">
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="loading" @click="refreshState">
            Refresh state
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="externalActionDisabled" @click="runAction('discover', Iec61850ClientAPI.discoverIed)">
            {{ busyAction === 'discover' ? 'Discovering...' : 'Discover' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('connect', Iec61850ClientAPI.connectIed)">
            {{ busyAction === 'connect' ? 'Connecting...' : 'Connect' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="externalActionDisabled" @click="runAction('disconnect', Iec61850ClientAPI.disconnectIed)">
            {{ busyAction === 'disconnect' ? 'Disconnecting...' : 'Disconnect' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('close-ied', Iec61850ClientAPI.closeIed)">
            {{ busyAction === 'close-ied' ? 'Closing IED...' : 'Close IED' }}
          </UiButton>
        </div>
        <div class="iec61850-client-page__action-group">
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="externalActionDisabled" @click="runAction('rptena', Iec61850ClientAPI.enableReporting)">
            {{ busyAction === 'rptena' ? 'Subscribing...' : 'RptEna' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="externalActionDisabled" @click="runAction('gi', Iec61850ClientAPI.sendGeneralInterrogation)">
            {{ busyAction === 'gi' ? 'Requesting GI...' : 'GI' }}
          </UiButton>
        </div>
        <div class="iec61850-client-page__action-group">
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="wireStartDisabled" @click="runAction('wire-start', Iec61850ClientAPI.startWireTransport)">
            {{ busyAction === 'wire-start' ? 'Starting wire...' : 'Start wire' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('wire-emit', Iec61850ClientAPI.emitWireReport)">
            {{ busyAction === 'wire-emit' ? 'Emitting report...' : 'Emit wire report' }}
          </UiButton>
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('wire-stop', Iec61850ClientAPI.stopWireTransport)">
            {{ busyAction === 'wire-stop' ? 'Stopping wire...' : 'Stop wire transport' }}
          </UiButton>
        </div>
        <div class="iec61850-client-page__action-group">
          <UiButton variant="toolbar" size="xs" class="iec61850-client-page__action" :disabled="busyAction !== null" @click="runAction('clear', Iec61850ClientAPI.clearTranscript)">
            {{ busyAction === 'clear' ? 'Clearing...' : 'Clear transcript' }}
          </UiButton>
        </div>
      </div>
    </header>

    <section v-if="errorMessage" class="iec61850-client-page__alert">
      {{ errorMessage }}
    </section>

    <section class="iec61850-client-page__summary">
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Session</span>
        <span class="iec61850-client-page__metric-value"><span class="iec61850-client-page__phase" :class="phaseClass">{{ sessionStatus }}</span></span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Discovery</span>
        <span class="iec61850-client-page__metric-value">{{ discoveryStatus }}</span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Selected RCB</span>
        <span class="iec61850-client-page__metric-value">{{ selectedReportStatus }}</span>
      </div>
      <div class="iec61850-client-page__metric">
        <span class="iec61850-client-page__metric-label">Selected DataSet</span>
        <span class="iec61850-client-page__metric-value">{{ selectedDatasetStatus }}</span>
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
          <h2>Runtime state contract</h2>
        </div>
        <div class="iec61850-client-page__panel-body iec61850-client-page__panel-body--scroll">
          <div v-if="uiState" class="iec61850-client-page__contract">
            <section class="iec61850-client-page__contract-section">
              <h3>Session</h3>
              <dl class="iec61850-client-page__state-list">
                <div><dt>Phase</dt><dd><span class="iec61850-client-page__phase" :class="phaseClass">{{ uiState.session.phase }}</span></dd></div>
                <div><dt>Endpoint</dt><dd>{{ uiState.session.endpoint_label }}</dd></div>
                <div><dt>Associated</dt><dd>{{ uiState.session.associated ? 'yes' : 'no' }}</dd></div>
                <div><dt>Last event</dt><dd>{{ uiState.session.last_event_kind ?? 'none' }}</dd></div>
              </dl>
            </section>

            <section class="iec61850-client-page__contract-section">
              <h3>Discovery</h3>
              <dl class="iec61850-client-page__state-list iec61850-client-page__state-list--grid">
                <div><dt>Logical devices</dt><dd>{{ uiState.discovery.logical_devices }}</dd></div>
                <div><dt>Logical nodes</dt><dd>{{ uiState.discovery.logical_nodes }}</dd></div>
                <div><dt>DataSets</dt><dd>{{ uiState.discovery.data_sets }}</dd></div>
                <div><dt>Members</dt><dd>{{ uiState.discovery.data_set_members }}</dd></div>
                <div><dt>Reports</dt><dd>{{ uiState.discovery.report_controls }}</dd></div>
                <div><dt>Signals</dt><dd>{{ uiState.discovery.signals }}</dd></div>
              </dl>
              <dl class="iec61850-client-page__state-list">
                <div><dt>Selected RCB</dt><dd>{{ uiState.discovery.selected_rcb_ref }}</dd></div>
                <div><dt>Selected DataSet</dt><dd>{{ uiState.discovery.selected_dataset_ref ?? 'none' }}</dd></div>
              </dl>
            </section>

            <section class="iec61850-client-page__contract-section">
              <h3>Subscription</h3>
              <dl class="iec61850-client-page__state-list">
                <div><dt>Status</dt><dd>{{ uiState.subscription.runtime_status }}</dd></div>
                <div><dt>Last command</dt><dd>{{ subscriptionCommandStatus }}</dd></div>
                <div><dt>Probe accepted</dt><dd>{{ uiState.subscription.command_accepted ? 'yes' : 'no' }}</dd></div>
                <div><dt>RptEna</dt><dd>{{ uiState.subscription.rptena_enabled ? 'enabled' : 'disabled' }}</dd></div>
                <div><dt>Owner</dt><dd>{{ uiState.subscription.owner ?? 'none' }}</dd></div>
                <div><dt>Reserved by</dt><dd>{{ uiState.subscription.reserved_by ?? 'none' }}</dd></div>
              </dl>
            </section>

            <section class="iec61850-client-page__contract-section">
              <h3>Last report</h3>
              <dl class="iec61850-client-page__state-list">
                <div><dt>RptID</dt><dd>{{ uiState.report.rpt_id ?? 'none' }}</dd></div>
                <div><dt>DataSet</dt><dd>{{ uiState.report.data_set_ref ?? 'none' }}</dd></div>
                <div><dt>Reason</dt><dd>{{ uiState.report.reason ?? 'none' }}</dd></div>
                <div><dt>Values</dt><dd>{{ uiState.report.matched_value_count }} matched / {{ uiState.report.unmatched_value_count }} unmatched</dd></div>
              </dl>
              <div v-if="reportValues.length" class="iec61850-client-page__report-values">
                <article v-for="value in reportValues" :key="`${value.index}-${value.reference}`" class="iec61850-client-page__report-value">
                  <strong>{{ value.reference }}</strong>
                  <span>{{ value.value ?? 'null' }} · {{ value.reason }}</span>
                </article>
              </div>
            </section>

            <section v-if="uiState.diagnostic.active" class="iec61850-client-page__contract-section iec61850-client-page__contract-section--diagnostic">
              <h3>Diagnostic</h3>
              <dl class="iec61850-client-page__state-list">
                <div><dt>Action</dt><dd>{{ uiState.diagnostic.action }}</dd></div>
                <div><dt>Code</dt><dd>{{ uiState.diagnostic.code }}</dd></div>
                <div><dt>Message</dt><dd>{{ uiState.diagnostic.message }}</dd></div>
              </dl>
            </section>
          </div>
          <p v-else class="iec61850-client-page__empty">No state loaded.</p>

          <details class="iec61850-client-page__raw-state">
            <summary>Raw snapshot JSON</summary>
            <pre class="iec61850-client-page__json">{{ state ? formatJson(state) : 'No state loaded' }}</pre>
          </details>
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
  gap: 0.5rem;
  justify-content: flex-end;
  max-width: min(100%, 68rem);
}

.iec61850-client-page__action-group {
  display: inline-flex;
  flex-wrap: nowrap;
  gap: 0.35rem;
  align-items: center;
  min-width: 0;
}

.iec61850-client-page__target-group {
  display: grid;
  grid-template-columns: minmax(8rem, 1fr) 5.2rem minmax(8rem, 1fr) minmax(14rem, 1.7fr) auto auto;
  gap: 0.35rem;
  align-items: center;
  width: min(100%, 64rem);
}

.iec61850-client-page__target-input {
  min-width: 0;
  height: 2rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: 8px;
  padding: 0 0.55rem;
  font-size: 0.78rem;
  color: var(--color-neutral-900);
  background: var(--color-white);
}

.iec61850-client-page__target-input--port {
  text-align: right;
}

.iec61850-client-page__target-input--path {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", monospace;
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

.iec61850-client-page__phase {
  display: inline-flex;
  align-items: center;
  min-height: 1.45rem;
  padding: 0.12rem 0.5rem;
  border-radius: 999px;
  border: 1px solid var(--color-neutral-300);
  background: var(--color-neutral-100);
  color: var(--color-neutral-800);
  font-size: 0.78rem;
  font-weight: 700;
  text-transform: uppercase;
}

.iec61850-client-page__phase--associated,
.iec61850-client-page__phase--discovered {
  border-color: color-mix(in srgb, #2563eb 35%, var(--color-neutral-200));
  background: color-mix(in srgb, #2563eb 10%, var(--color-white));
  color: #1d4ed8;
}

.iec61850-client-page__phase--subscribed,
.iec61850-client-page__phase--rptena-accepted,
.iec61850-client-page__phase--gi-accepted,
.iec61850-client-page__phase--reporting {
  border-color: color-mix(in srgb, #16a34a 35%, var(--color-neutral-200));
  background: color-mix(in srgb, #16a34a 10%, var(--color-white));
  color: #15803d;
}

.iec61850-client-page__phase--failed {
  border-color: var(--color-red-200);
  background: var(--color-red-50);
  color: var(--color-red-800);
}

.iec61850-client-page__contract {
  display: grid;
  gap: 0.9rem;
  min-width: 0;
}

.iec61850-client-page__contract-section {
  display: grid;
  gap: 0.55rem;
  min-width: 0;
  padding-bottom: 0.85rem;
  border-bottom: 1px solid var(--color-neutral-200);
}

.iec61850-client-page__contract-section:last-child {
  border-bottom: 0;
  padding-bottom: 0;
}

.iec61850-client-page__contract-section h3 {
  margin: 0;
  color: var(--color-neutral-600);
  font-size: 0.72rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.iec61850-client-page__contract-section--diagnostic {
  border-color: var(--color-red-200);
}

.iec61850-client-page__state-list {
  display: grid;
  gap: 0.35rem;
  margin: 0;
  min-width: 0;
}

.iec61850-client-page__state-list--grid {
  grid-template-columns: repeat(3, minmax(0, 1fr));
}

.iec61850-client-page__state-list div {
  display: grid;
  grid-template-columns: minmax(7.5rem, 0.45fr) minmax(0, 1fr);
  gap: 0.55rem;
  align-items: start;
  min-width: 0;
}

.iec61850-client-page__state-list--grid div {
  grid-template-columns: 1fr;
  gap: 0.12rem;
}

.iec61850-client-page__state-list dt {
  color: var(--color-neutral-500);
  font-size: 0.72rem;
}

.iec61850-client-page__state-list dd {
  margin: 0;
  min-width: 0;
  color: var(--color-neutral-900);
  font-size: 0.82rem;
  font-weight: 600;
  overflow-wrap: anywhere;
}

.iec61850-client-page__report-values {
  display: grid;
  gap: 0.35rem;
  min-width: 0;
}

.iec61850-client-page__report-value {
  display: grid;
  gap: 0.15rem;
  min-width: 0;
  padding: 0.35rem 0;
  border-top: 1px solid var(--color-neutral-100);
}

.iec61850-client-page__report-value strong,
.iec61850-client-page__report-value span {
  min-width: 0;
  overflow-wrap: anywhere;
  font-size: 0.78rem;
}

.iec61850-client-page__report-value span {
  color: var(--color-neutral-600);
}

.iec61850-client-page__raw-state {
  margin-top: 1rem;
  min-width: 0;
}

.iec61850-client-page__raw-state summary {
  cursor: pointer;
  color: var(--color-neutral-600);
  font-size: 0.78rem;
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
}

.iec61850-client-page__raw-state .iec61850-client-page__json {
  margin-top: 0.65rem;
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

  .iec61850-client-page__state-list--grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .iec61850-client-page__state-list div {
    grid-template-columns: 1fr;
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

:global(.dark .iec61850-client-page__phase) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-client-page__contract-section) {
  border-color: var(--color-neutral-800);
}

:global(.dark .iec61850-client-page__contract-section h3),
:global(.dark .iec61850-client-page__state-list dt),
:global(.dark .iec61850-client-page__report-value span),
:global(.dark .iec61850-client-page__raw-state summary) {
  color: var(--color-neutral-400);
}

:global(.dark .iec61850-client-page__state-list dd),
:global(.dark .iec61850-client-page__report-value strong) {
  color: var(--color-neutral-100);
}

:global(.dark .iec61850-client-page__report-value) {
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
