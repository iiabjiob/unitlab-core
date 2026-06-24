<template>
  <section class="core-network-panel">
    <div class="core-network-panel__header">
      <div class="core-network-panel__heading">
        <p class="core-network-panel__eyebrow">Network / RJ45</p>
        <p class="core-network-panel__meta">
          Configured interface:
          <span class="core-network-panel__meta-strong">{{ configuredInterfaceLabel }}</span>
          · Mode:
          <span class="core-network-panel__meta-strong">{{ hostModeLabel }}</span>
          <template v-if="networkSnapshot?.last_event"> · {{ networkSnapshot.last_event }}</template>
        </p>
        <p class="core-network-panel__subtle">
          Select the active host interface, then review its live IP, mask, network and connection state before applying MMS/SCADA settings.
          The recommendation follows the live default route when the host reports one.
        </p>
        <p v-if="networkErrorText" class="core-network-panel__error">{{ networkErrorText }}</p>
      </div>
    </div>

    <div class="core-network-panel__grid">
      <div class="core-network-panel__card">
        <div class="core-network-panel__card-header">
          <p class="core-network-panel__card-title">Host network settings</p>
          <span class="core-network-panel__card-meta">{{ networkSnapshot?.updated_at || '—' }}</span>
        </div>

        <div class="core-network-panel__form-grid">
          <label class="core-network-panel__field core-network-panel__field--wide">
            <span class="core-network-panel__field-label">Interface</span>
            <select v-model="draft.interface" class="core-network-panel__input" @change="markDirty">
              <option v-for="choice in interfaceChoices" :key="choice.value" :value="choice.value">
                {{ choice.label }}
              </option>
            </select>
          </label>

          <label class="core-network-panel__field">
            <span class="core-network-panel__field-label">Profile</span>
            <input v-model.trim="draft.profile" class="core-network-panel__input" type="text" autocomplete="off" @input="markDirty" />
          </label>

          <label class="core-network-panel__field">
            <span class="core-network-panel__field-label">IPv4 mode</span>
            <select v-model="draft.ipv4Mode" class="core-network-panel__input" @change="markDirty">
              <option value="auto">DHCP / auto</option>
              <option value="manual">Manual static</option>
            </select>
          </label>

          <label class="core-network-panel__field">
            <span class="core-network-panel__field-label">IP address</span>
            <input v-model.trim="draft.ipAddress" class="core-network-panel__input" type="text" autocomplete="off" placeholder="192.168.10.21" @input="markDirty" />
          </label>

          <label class="core-network-panel__field">
            <span class="core-network-panel__field-label">Subnet prefix</span>
            <input v-model.trim="draft.prefixLength" class="core-network-panel__input" type="number" min="1" max="32" step="1" placeholder="24" @input="markDirty" />
          </label>

          <label class="core-network-panel__field">
            <span class="core-network-panel__field-label">Gateway</span>
            <input v-model.trim="draft.gateway" class="core-network-panel__input" type="text" autocomplete="off" placeholder="192.168.10.1" @input="markDirty" />
          </label>

          <label class="core-network-panel__field core-network-panel__field--wide">
            <span class="core-network-panel__field-label">DNS servers</span>
            <input v-model.trim="draft.dnsServers" class="core-network-panel__input" type="text" autocomplete="off" placeholder="192.168.10.1, 1.1.1.1" @input="markDirty" />
          </label>

          <label class="core-network-panel__field core-network-panel__field--wide">
            <span class="core-network-panel__field-label">Proxy URL</span>
            <input v-model.trim="draft.proxyUrl" class="core-network-panel__input" type="text" autocomplete="off" placeholder="http://proxy:3128" @input="markDirty" />
          </label>

          <label class="core-network-panel__field core-network-panel__field--wide">
            <span class="core-network-panel__field-label">No proxy hosts</span>
            <input v-model.trim="draft.proxyNoProxy" class="core-network-panel__input" type="text" autocomplete="off" placeholder="localhost, 127.0.0.1, 10.0.0.0/8" @input="markDirty" />
          </label>
        </div>

        <div class="core-network-panel__selected-card">
          <div class="core-network-panel__selected-card-header">
            <p class="core-network-panel__card-title">Selected interface details</p>
            <span class="core-network-panel__card-meta">{{ selectedInterfaceSummary || 'No live details yet' }}</span>
          </div>

          <div v-if="selectedInterface" class="core-network-panel__facts">
            <div class="core-network-panel__fact-row">
              <span class="core-network-panel__fact-label">Device type</span>
              <span class="core-network-panel__fact-value">{{ selectedInterface.device_type || '—' }}</span>
            </div>
            <div class="core-network-panel__fact-row">
              <span class="core-network-panel__fact-label">IP address</span>
              <span class="core-network-panel__fact-value">{{ selectedInterface.local_ip || '—' }}</span>
            </div>
            <div class="core-network-panel__fact-row">
              <span class="core-network-panel__fact-label">Mask</span>
              <span class="core-network-panel__fact-value">{{ selectedInterface.netmask ? `/${selectedInterface.netmask}` : '—' }}</span>
            </div>
            <div class="core-network-panel__fact-row">
              <span class="core-network-panel__fact-label">Network</span>
              <span class="core-network-panel__fact-value">{{ selectedInterface.network || '—' }}</span>
            </div>
            <div class="core-network-panel__fact-row">
              <span class="core-network-panel__fact-label">Connection</span>
              <span class="core-network-panel__fact-value">{{ selectedInterface.connection || '—' }}</span>
            </div>
            <div class="core-network-panel__fact-row">
              <span class="core-network-panel__fact-label">State</span>
              <span class="core-network-panel__fact-value">{{ selectedInterface.state || '—' }}</span>
            </div>
          </div>
          <div v-if="selectedInterfaceWarnings.length" class="core-network-panel__warnings">
            <p class="core-network-panel__warnings-title">Interface warnings</p>
            <ul>
              <li v-for="warning in selectedInterfaceWarnings" :key="warning">{{ warning }}</li>
            </ul>
          </div>
          <p v-else-if="!selectedInterface" class="core-network-panel__empty">
            No live details for the selected interface yet.
          </p>
        </div>

        <div class="core-network-panel__actions">
          <button type="button" class="btn btn-base btn-secondary" :disabled="networkBusy" @click="refreshNetworkInterfaces">
            Refresh interfaces
          </button>
          <button type="button" class="btn btn-base btn-secondary" :disabled="networkBusy" @click="resetDraft">
            Reset
          </button>
          <button type="button" class="btn btn-base btn-secondary" :disabled="networkBusy" @click="useDhcpDefaults">
            Use DHCP defaults
          </button>
          <button type="button" class="btn btn-base btn-success" :disabled="networkBusy || !canApplySettings" @click="applyNetworkSettings">
            Apply settings
          </button>
          <span class="core-network-panel__action-meta">
            {{ draftSummary }}
          </span>
        </div>

        <div v-if="networkSnapshot?.request_in_flight" class="core-network-panel__in-flight">
          In progress: {{ networkSnapshot.request_in_flight.action }} ({{ networkSnapshot.request_in_flight.request_id }})
        </div>
      </div>

      <div class="core-network-panel__card">
        <div class="core-network-panel__card-header">
          <p class="core-network-panel__card-title">Observed interfaces</p>
          <span class="core-network-panel__card-meta">{{ interfaces.length }} interface{{ interfaces.length === 1 ? '' : 's' }}</span>
        </div>

        <div class="core-network-panel__interfaces">
          <div v-for="entry in interfaces" :key="entry.interface_name" class="core-network-panel__interface-row">
            <div class="core-network-panel__interface-main">
              <span class="core-network-panel__interface-name">{{ entry.interface_name }}</span>
              <span class="core-network-panel__interface-state">{{ entry.device_type || 'unknown' }}</span>
              <span v-if="entry.is_default_route || recommendedInterface?.interface_name === entry.interface_name" class="core-network-panel__interface-badge">recommended</span>
              <span v-if="entry.connection" class="core-network-panel__interface-connection">{{ entry.connection }}</span>
            </div>
            <div class="core-network-panel__interface-meta">
              <span>{{ entry.local_ip || '—' }}</span>
              <span>{{ entry.network || '—' }}</span>
              <span>{{ entry.netmask ? `/${entry.netmask}` : '—' }}</span>
              <span v-if="entry.carrier === false" class="core-network-panel__interface-warning">no carrier</span>
              <span v-if="!entry.local_ip" class="core-network-panel__interface-warning">no IPv4</span>
              <span v-if="entry.oper_state" class="core-network-panel__interface-operstate">{{ entry.oper_state }}</span>
            </div>
          </div>
          <p v-if="interfaces.length === 0" class="core-network-panel__empty">
            No interface snapshot reported yet.
          </p>
        </div>

        <div class="core-network-panel__notes">
          <p class="core-network-panel__notes-title">Operator notes</p>
          <ul>
            <li>Use RJ45 for the MMS/BCU target network.</li>
            <li>Keep the Wi-Fi AP available for local access and fallback.</li>
            <li>If the interface is missing, plug the cable first and re-run status.</li>
          </ul>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, reactive, ref, watch } from "vue"
import { useCoreNetworkStore } from "@/stores/coreNetworkStore"
import {
  buildCoreNetworkInterfaceChoices,
  buildCoreNetworkInterfaceWarnings,
  buildCoreNetworkSettingsPayload,
  buildRecommendedCoreNetworkInterface,
  createCoreNetworkDraft,
  getSelectedCoreNetworkInterface,
} from "./coreNetworkPanelModel"

const coreNetworkStore = useCoreNetworkStore()

const networkSnapshot = computed(() => coreNetworkStore.snapshot)
const hostNetwork = computed(() => coreNetworkStore.hostNetwork)
const interfaces = computed(() => coreNetworkStore.interfaces)
const networkBusy = computed(() => coreNetworkStore.commandPending || coreNetworkStore.loading)
const networkErrorText = computed(() => coreNetworkStore.lastError || networkSnapshot.value?.last_error || hostNetwork.value?.last_error || null)
const configuredInterfaceLabel = computed(() => hostNetwork.value?.interface || networkSnapshot.value?.wifi_iface || "eth0")
const hostModeLabel = computed(() => hostNetwork.value?.ipv4_mode?.toUpperCase() || "AUTO")

const draft = reactive(createCoreNetworkDraft(hostNetwork.value, configuredInterfaceLabel.value))
const draftDirty = ref(false)

const recommendedInterface = computed(() => buildRecommendedCoreNetworkInterface(interfaces.value, configuredInterfaceLabel.value))

function syncDraftFromSnapshot() {
  if (draftDirty.value) return
  const next = createCoreNetworkDraft(hostNetwork.value, recommendedInterface.value?.interface_name || configuredInterfaceLabel.value)
  draft.interface = next.interface
  draft.profile = next.profile
  draft.ipv4Mode = next.ipv4Mode
  draft.ipAddress = next.ipAddress
  draft.prefixLength = next.prefixLength
  draft.gateway = next.gateway
  draft.dnsServers = next.dnsServers
  draft.proxyUrl = next.proxyUrl
  draft.proxyNoProxy = next.proxyNoProxy
}

function markDirty() {
  draftDirty.value = true
}

function resetDraft() {
  draftDirty.value = false
  syncDraftFromSnapshot()
}

function useDhcpDefaults() {
  draft.ipv4Mode = "auto"
  draft.ipAddress = ""
  draft.prefixLength = "24"
  draft.gateway = ""
  draft.dnsServers = ""
  markDirty()
}

async function applyNetworkSettings() {
  await coreNetworkStore.applySettings(buildCoreNetworkSettingsPayload(draft))
  draftDirty.value = false
}

const interfaceChoices = computed(() => buildCoreNetworkInterfaceChoices(interfaces.value, draft.interface))
const selectedInterface = computed(() => getSelectedCoreNetworkInterface(interfaces.value, draft.interface))
const selectedInterfaceWarnings = computed(() => buildCoreNetworkInterfaceWarnings(selectedInterface.value))
const selectedInterfaceSummary = computed(() => {
  const item = selectedInterface.value
  if (!item) return null
  const parts: string[] = []
  if (item.state) parts.push(item.state)
  if (item.connection) parts.push(item.connection)
  if (item.device_type) parts.push(item.device_type)
  if (item.is_default_route) parts.push("default route")
  return parts.length ? parts.join(" · ") : null
})
const canApplySettings = computed(() => {
  if (networkBusy.value) return false
  if (!draft.interface.trim() || !draft.profile.trim()) return false
  if (draft.ipv4Mode === "manual") {
    return Boolean(draft.ipAddress.trim())
  }
  return true
})

const draftSummary = computed(() => {
  const items: string[] = []
  items.push(draftDirty.value ? "edited" : "synced")
  items.push(draft.ipv4Mode === "manual" ? "manual IPv4" : "DHCP")
  if (draft.proxyUrl.trim()) items.push("proxy set")
  return items.join(" · ")
})

watch(hostNetwork, () => {
  syncDraftFromSnapshot()
}, { immediate: true })

watch(interfaces, () => {
  if (!draftDirty.value) {
    syncDraftFromSnapshot()
  }
})

async function refreshNetworkInterfaces() {
  await coreNetworkStore.refreshInterfaces()
}

onMounted(() => {
  coreNetworkStore.startMonitoring()
})

onUnmounted(() => {
  coreNetworkStore.stopMonitoring()
})
</script>

<style scoped>
.core-network-panel {
  display: flex;
  min-width: 0;
  min-height: 0;
  flex-direction: column;
  gap: 1rem;
}

.core-network-panel__header {
  padding-bottom: 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
}

.core-network-panel__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  text-transform: uppercase;
}

.core-network-panel__meta,
.core-network-panel__subtle {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.core-network-panel__meta-strong {
  color: var(--color-neutral-900);
  font-weight: 600;
}

.core-network-panel__error {
  margin-top: 0.35rem;
  color: var(--color-red-600);
  font-size: var(--text-xs);
}

.core-network-panel__grid {
  display: grid;
  grid-template-columns: minmax(0, 1.2fr) minmax(0, 0.8fr);
  gap: 1rem;
  min-width: 0;
}

.core-network-panel__card {
  min-width: 0;
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 14px;
  background: var(--color-white);
}

.core-network-panel__card-header,
.core-network-panel__selected-card-header {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 1rem;
  margin-bottom: 1rem;
}

.core-network-panel__selected-card {
  margin-top: 1rem;
  padding: 0.85rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 12px;
  background: color-mix(in srgb, var(--color-neutral-50) 60%, transparent);
}

.core-network-panel__card-title {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 600;
}

.core-network-panel__card-meta {
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-network-panel__form-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.75rem;
}

.core-network-panel__field {
  display: flex;
  flex-direction: column;
  gap: 0.3rem;
}

.core-network-panel__field--wide {
  grid-column: 1 / -1;
}

.core-network-panel__field-label {
  color: var(--color-neutral-500);
  font-size: 11px;
  text-transform: uppercase;
}

.core-network-panel__input {
  width: 100%;
  min-width: 0;
  padding: 0.65rem 0.75rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: 10px;
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
}

.core-network-panel__input:focus {
  outline: none;
  border-color: var(--color-primary-500);
  box-shadow: 0 0 0 3px color-mix(in srgb, var(--color-primary-500) 20%, transparent);
}

.core-network-panel__actions {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-top: 1rem;
}

.core-network-panel__action-meta {
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-network-panel__in-flight {
  margin-top: 0.75rem;
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-network-panel__facts {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 0.65rem;
}

.core-network-panel__fact-row {
  display: flex;
  flex-direction: column;
  gap: 0.2rem;
  padding: 0.55rem 0.65rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 10px;
  background: var(--color-white);
}

.core-network-panel__fact-label {
  color: var(--color-neutral-500);
  font-size: 11px;
  text-transform: uppercase;
}

.core-network-panel__fact-value {
  color: var(--color-neutral-900);
  font-size: var(--text-xs);
  font-weight: 600;
}

.core-network-panel__interfaces {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.core-network-panel__interface-row {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 12px;
  background: color-mix(in srgb, var(--color-neutral-50) 60%, transparent);
}

.core-network-panel__interface-main,
.core-network-panel__interface-meta {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  align-items: center;
}

.core-network-panel__interface-name,
.core-network-panel__interface-state,
.core-network-panel__interface-connection {
  font-size: var(--text-xs);
}

.core-network-panel__interface-name {
  color: var(--color-neutral-900);
  font-weight: 600;
}

.core-network-panel__interface-state,
.core-network-panel__interface-connection,
.core-network-panel__interface-operstate,
.core-network-panel__interface-badge,
.core-network-panel__interface-warning {
  color: var(--color-neutral-500);
}

.core-network-panel__interface-badge {
  padding: 0.1rem 0.45rem;
  border-radius: 999px;
  background: color-mix(in srgb, var(--color-primary-100) 70%, transparent);
  color: var(--color-primary-700);
  font-size: 10px;
  font-weight: 600;
  text-transform: uppercase;
}

.core-network-panel__interface-warning {
  color: var(--color-amber-700);
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
}

.core-network-panel__interface-operstate {
  font-size: 11px;
}

.core-network-panel__interface-meta {
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-network-panel__notes {
  margin-top: 1rem;
  padding-top: 1rem;
  border-top: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
}

.core-network-panel__notes-title {
  margin-bottom: 0.5rem;
  color: var(--color-neutral-900);
  font-size: var(--text-xs);
  font-weight: 600;
}

.core-network-panel__notes ul {
  padding-left: 1rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.core-network-panel__warnings {
  margin-top: 0.75rem;
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-amber-300) 65%, transparent);
  border-radius: 10px;
  background: color-mix(in srgb, var(--color-amber-50) 60%, transparent);
}

.core-network-panel__warnings-title {
  margin-bottom: 0.35rem;
  color: var(--color-amber-800);
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
}

.core-network-panel__warnings ul {
  padding-left: 1rem;
  color: var(--color-amber-900);
  font-size: var(--text-xs);
}

.core-network-panel__empty {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

:global(.dark .core-network-panel__header){
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .core-network-panel__meta-strong),
:global(.dark .core-network-panel__card-title),
:global(.dark .core-network-panel__fact-value),
:global(.dark .core-network-panel__interface-name),
:global(.dark .core-network-panel__notes-title){
  color: var(--color-neutral-100);
}

:global(.dark .core-network-panel__card),
:global(.dark .core-network-panel__fact-row),
:global(.dark .core-network-panel__input){
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark .core-network-panel__selected-card),
:global(.dark .core-network-panel__interface-row){
  background: color-mix(in srgb, var(--color-neutral-900) 80%, transparent);
  border-color: var(--color-neutral-800);
}

@media (max-width: 1100px) {
  .core-network-panel__grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 800px) {
  .core-network-panel__form-grid,
  .core-network-panel__facts {
    grid-template-columns: 1fr;
  }
}
</style>
