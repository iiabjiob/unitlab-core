<template>
  <div class="home-page">
    <main class="home-page__shell">
      <header class="home-page__header">
        <div class="home-page__brand">
          <AppLogo class="home-page__logo" />
          <div class="home-page__brand-meta">
            <TimeComponent />
            <OnlineStatusComponent :status="systemStatus" :description="systemStatusDescription" neutral-offline />
          </div>
        </div>

        <div class="home-page__header-actions">
          <div class="home-page__workspace-switcher">
            <WorkspaceSwitcher variant="compact" />
          </div>
          <ThemeToggle />
        </div>
      </header>

      <section class="home-page__overview">
        <div class="home-page__workspace-panel">
          <p class="home-page__eyebrow">Active workspace</p>
          <h1 class="home-page__workspace-title">{{ activeWorkspaceName }}</h1>
          <p class="home-page__workspace-summary">{{ workspaceSummary }}</p>
          <p v-if="workspaceError" class="home-page__workspace-error">{{ workspaceError }}</p>

          <div
            class="home-page__signal-progress"
            :style="{ '--home-page-signal-progress': signalProgressStyle }"
          >
            <div class="home-page__signal-progress-header">
              <span>{{ signalProgressTitle }}</span>
              <strong>{{ signalProgressPercent }}%</strong>
            </div>
            <div class="home-page__signal-progress-meter" aria-hidden="true">
              <span />
            </div>
            <p>{{ signalProgressDetail }}</p>
          </div>

          <div class="home-page__primary-actions">
            <button
              type="button"
              class="btn btn-primary btn-lg"
              @click="goTo(recommendedAction.route)"
            >
              <svg
                aria-hidden="true"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                stroke-width="1.8"
                stroke-linecap="round"
                stroke-linejoin="round"
              >
                <path d="M5 12h14" />
                <path d="m13 6 6 6-6 6" />
              </svg>
              {{ recommendedAction.label }}
            </button>
            <button
              type="button"
              class="btn btn-secondary btn-lg"
              @click="goTo({ name: 'devices.list' })"
            >
              Hardware
            </button>
          </div>
        </div>

        <div class="home-page__metrics">
          <article
            v-for="metric in metrics"
            :key="metric.label"
            class="home-page__metric"
            :class="`home-page__metric--${metric.tone}`"
          >
            <span class="home-page__metric-label">{{ metric.label }}</span>
            <strong class="home-page__metric-value">{{ metric.value }}</strong>
            <span class="home-page__metric-detail">{{ metric.detail }}</span>
          </article>
        </div>
      </section>

      <section class="home-page__workbench">
        <div class="home-page__section-header">
          <div>
            <p class="home-page__eyebrow">Workbench</p>
            <h2 class="home-page__section-title">FAT runtime paths</h2>
          </div>
        </div>

        <div class="home-page__quick-grid">
          <button
            v-for="scenario in scenarioCards"
            :key="scenario.title"
            type="button"
            class="home-page__scenario"
            :class="`home-page__scenario--${scenario.tone}`"
            @click="goTo(scenario.route)"
          >
            <span class="home-page__scenario-index">{{ scenario.step }}</span>
            <span class="home-page__scenario-content">
              <span class="home-page__scenario-title">{{ scenario.title }}</span>
              <span class="home-page__scenario-description">{{ scenario.description }}</span>
              <span class="home-page__scenario-meta">{{ scenario.metric }}</span>
            </span>
            <svg
              class="home-page__scenario-arrow"
              aria-hidden="true"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.8"
              stroke-linecap="round"
              stroke-linejoin="round"
            >
              <path d="M5 12h14" />
              <path d="m13 6 6 6-6 6" />
            </svg>
          </button>
        </div>
      </section>
    </main>
  </div>
</template>

<script setup lang="ts">
import { computed, watch } from "vue"
import { useRouter, type RouteLocationRaw } from "vue-router"
import AppLogo from "@/components/layout/AppLogo.vue"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"
import TimeComponent from "@/components/misc/TimeComponent.vue"
import ThemeToggle from "@/components/ui/ThemeToggle.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

const workspaceStore = useWorkspaceStore()
const systemHealthStore = useSystemHealthStore()
const deviceStore = useDeviceStore()
const channelStore = useChannelStore()
const signalSheetStore = useSignalSheetStore()
const router = useRouter()
const integerFormatter = new Intl.NumberFormat()

const systemStatus = computed(() => systemHealthStore.status)
const systemStatusDescription = computed(() => (
  systemStatus.value === "degraded" ? systemHealthStore.tooltip : null
))
const onlineDevicesCount = computed(() => deviceStore.devices.filter((device) => device.status === "online").length)
const totalDevicesCount = computed(() => deviceStore.devices.length)
const totalChannelsCount = computed(() => channelStore.channels.length)
const signalCount = computed(() => Math.max(0, Number(signalSheetStore.sheet?.signals_count ?? 0)))
const testedSignalCount = computed(() => {
  if (!signalSheetStore.allocationRows.length) {
    return 0
  }
  return signalSheetStore.allocationRows.reduce((count, row) => (
    count + (String(row.tested_at ?? "").trim() ? 1 : 0)
  ), 0)
})
const remainingSignalCount = computed(() => Math.max(0, signalCount.value - testedSignalCount.value))
const activeWorkspaceName = computed(() => workspaceStore.activeWorkspace?.name ?? "Workspace")

const workspaceSummary = computed(() => {
  if (workspaceStore.loading) {
    return "Syncing workspace state..."
  }

  if (!workspaceStore.workspaces.length) {
    return "Default workspace is being prepared."
  }

  if (signalSheetStore.hasSheet) {
    return "Signal list is ready for FAT test tracking."
  }

  return "Import a signal list to start FAT preparation."
})

const workspaceError = computed(() => workspaceStore.error)

type HomeRoute = RouteLocationRaw
type HomeAction = { label: string; route: HomeRoute }
type MetricTone = "blue" | "green" | "amber" | "neutral"
type HomeMetric = {
  label: string
  value: string
  detail: string
  tone: MetricTone
}
type QuickAction = {
  step: string
  title: string
  description: string
  route: HomeRoute
  metric: string
  tone: "hardware" | "signals" | "switchgears" | "sequences"
}

const recommendedAction = computed<HomeAction>(() => {
  if (signalSheetStore.hasSheet) {
    return {
      label: "Open signals",
      route: { name: "signals.home" },
    }
  }

  return {
    label: "Import signals",
    route: { name: "signals.home", query: { import: "1" } },
  }
})

const signalProgressPercent = computed(() => {
  if (!signalCount.value) {
    return 0
  }
  return Math.max(0, Math.min(100, Math.round((testedSignalCount.value / signalCount.value) * 100)))
})
const signalProgressStyle = computed(() => `${signalProgressPercent.value}%`)
const signalProgressTitle = computed(() => {
  if (!signalCount.value) {
    return "No signal list loaded"
  }
  return `${formatInteger(remainingSignalCount.value)} signals remaining`
})
const signalProgressDetail = computed(() => {
  if (signalSheetStore.loadingAllocations) {
    return "Loading test progress..."
  }
  if (!signalCount.value) {
    return "Import a signal list before running FAT checks."
  }
  return `${formatInteger(testedSignalCount.value)} tested of ${formatInteger(signalCount.value)} total.`
})

const metrics = computed<HomeMetric[]>(() => [
  {
    label: "Devices online",
    value: `${formatInteger(onlineDevicesCount.value)} / ${formatInteger(totalDevicesCount.value)}`,
    detail: totalDevicesCount.value ? "registered runtime units" : "no devices registered",
    tone: onlineDevicesCount.value > 0 ? "green" : "neutral",
  },
  {
    label: "Channels loaded",
    value: formatInteger(totalChannelsCount.value),
    detail: "available I/O endpoints",
    tone: totalChannelsCount.value > 0 ? "blue" : "neutral",
  },
  {
    label: "Signal rows",
    value: formatInteger(signalCount.value),
    detail: signalSheetStore.hasSheet ? "active signal list" : "awaiting import",
    tone: signalSheetStore.hasSheet ? "blue" : "amber",
  },
])

const scenarioCards = computed<QuickAction[]>(() => [
  {
    step: "01",
    title: "Live Hardware",
    description: "Devices, channels, diagnostics, direct I/O state.",
    metric: `${formatInteger(onlineDevicesCount.value)} online`,
    tone: "hardware",
    route: { name: "devices.list" },
  },
  {
    step: "02",
    title: "Signals",
    description: "Signal list editing and test status.",
    metric: signalCount.value ? `${formatInteger(remainingSignalCount.value)} remaining` : "Awaiting import",
    tone: "signals",
    route: { name: "signals.home" },
  },
  {
    step: "03",
    title: "Switchgears",
    description: "Position bindings and controlled switching checks.",
    metric: "Control flows",
    tone: "switchgears",
    route: { name: "switchgears.list" },
  },
  {
    step: "04",
    title: "Sequencer",
    description: "Repeatable instructions and execution evidence.",
    metric: "Run paths",
    tone: "sequences",
    route: { name: "instructions.list" },
  },
])

watch(
  () => workspaceStore.activeWorkspaceId,
  (workspaceId) => {
    if (!workspaceId) {
      return
    }
    void runStoreBootstrap(
      ["home-signal-state", workspaceId],
      [refreshHomeSignalState],
      { mode: "settled" },
    )
  },
  { immediate: true },
)

async function refreshHomeSignalState() {
  await signalSheetStore.ensureSheetLoaded({ ttlMs: 15_000 })
  if (signalSheetStore.hasSheet) {
    await signalSheetStore.ensureAllocationsLoaded({ ttlMs: 15_000 })
  }
}

function formatInteger(value: number) {
  return integerFormatter.format(Math.max(0, Number(value) || 0))
}

function goTo(route: HomeRoute) {
  void router.push(route)
}
</script>

<style scoped>
.home-page {
  position: relative;
  isolation: isolate;
  min-height: 100vh;
  min-height: 100svh;
  background:
    linear-gradient(180deg, var(--color-white) 0%, var(--color-neutral-50) 38%, var(--color-neutral-100) 100%);
  color: var(--color-neutral-900);
  overflow: hidden;
}

.home-page::before {
  position: fixed;
  inset: 0;
  z-index: 0;
  background:
    radial-gradient(circle at 12% 20%, rgb(37 99 235 / 0.14) 0 1px, transparent 2px),
    radial-gradient(circle at 34% 34%, rgb(23 23 23 / 0.11) 0 1px, transparent 2px),
    radial-gradient(circle at 72% 22%, rgb(37 99 235 / 0.1) 0 1px, transparent 2px),
    linear-gradient(90deg, rgb(37 99 235 / 0.055) 1px, transparent 1px),
    linear-gradient(0deg, rgb(23 23 23 / 0.04) 1px, transparent 1px);
  background-size:
    22rem 18rem,
    26rem 22rem,
    30rem 20rem,
    4.5rem 4.5rem,
    4.5rem 4.5rem;
  content: "";
  mask-image: linear-gradient(135deg, transparent 0%, black 14%, black 58%, transparent 100%);
  opacity: 0.75;
  pointer-events: none;
}

.home-page__shell {
  position: relative;
  z-index: 1;
  display: grid;
  width: min(100%, 76rem);
  min-height: 100vh;
  min-height: 100svh;
  align-content: start;
  gap: 1rem;
  margin: 0 auto;
  padding: 1rem;
}

.home-page__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 84%, transparent);
  border-radius: var(--radius-lg);
  background: color-mix(in srgb, var(--color-white) 94%, var(--color-neutral-50));
  box-shadow:
    0 16px 34px rgb(15 23 42 / 0.08),
    inset 0 1px 0 rgb(255 255 255 / 84%);
}

.home-page__brand {
  display: flex;
  align-items: center;
  gap: 1rem;
  min-width: 0;
}

.home-page__brand-meta {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.625rem;
  min-width: 0;
}

.home-page__logo :deep(.app-logo__mark) {
  width: 2.5rem;
  height: 2.5rem;
  line-height: 2.5rem;
}

.home-page__logo :deep(.app-logo__wordmark) {
  font-size: var(--text-2xl);
}

.home-page__header-actions {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 0.5rem;
  min-width: 0;
}

.home-page__workspace-switcher {
  width: min(22rem, 38vw);
  min-width: 14rem;
}

.home-page__overview {
  display: grid;
  gap: 1rem;
}

.home-page__workspace-panel,
.home-page__metric,
.home-page__scenario {
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 86%, transparent);
  border-radius: var(--radius-lg);
  background: color-mix(in srgb, var(--color-white) 94%, var(--color-neutral-50));
  box-shadow:
    0 14px 28px rgb(15 23 42 / 0.07),
    inset 0 1px 0 rgb(255 255 255 / 80%);
}

.home-page__workspace-panel {
  display: grid;
  align-content: start;
  gap: 0.875rem;
  padding: 1.25rem;
}

.home-page__eyebrow {
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 700;
  letter-spacing: 0.16em;
  text-transform: uppercase;
}

.home-page__workspace-title {
  color: var(--color-neutral-950);
  font-size: var(--text-3xl);
  font-weight: 800;
  letter-spacing: 0;
  line-height: 1.1;
}

.home-page__workspace-summary {
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
}

.home-page__workspace-error {
  color: var(--color-red-500);
  font-size: var(--text-sm);
}

.home-page__signal-progress {
  display: grid;
  gap: 0.5rem;
  padding: 0.875rem;
  border: 1px solid color-mix(in srgb, var(--color-blue-300) 42%, var(--color-neutral-200));
  border-radius: var(--radius-lg);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-blue-100) 38%, var(--color-white)), var(--color-white));
}

.home-page__signal-progress-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 700;
}

.home-page__signal-progress-header strong {
  color: var(--color-blue-600);
  font-size: var(--text-xs);
}

.home-page__signal-progress-meter {
  height: 0.5rem;
  overflow: hidden;
  border-radius: 999px;
  background: color-mix(in srgb, var(--color-neutral-200) 84%, var(--color-white));
}

.home-page__signal-progress-meter span {
  display: block;
  width: var(--home-page-signal-progress);
  height: 100%;
  border-radius: inherit;
  background: linear-gradient(90deg, var(--color-neutral-950), var(--color-blue-600));
  transition: width 180ms ease;
}

.home-page__signal-progress p {
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

.home-page__primary-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 0.625rem;
  padding-top: 0.25rem;
}

.home-page__primary-actions svg {
  width: 1rem;
  height: 1rem;
}

.home-page__metrics {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(10rem, 1fr));
  gap: 0.75rem;
}

.home-page__metric {
  position: relative;
  display: grid;
  gap: 0.35rem;
  min-height: 7.25rem;
  overflow: hidden;
  padding: 1rem;
}

.home-page__metric::before,
.home-page__scenario::before {
  position: absolute;
  inset: 0 auto 0 0;
  width: 0.25rem;
  background: var(--home-page-accent, var(--color-blue-600));
  content: "";
}

.home-page__metric-label,
.home-page__metric-detail,
.home-page__scenario-meta {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.home-page__metric-value {
  color: var(--color-neutral-950);
  font-size: var(--text-2xl);
  font-weight: 800;
  line-height: 1.1;
}

.home-page__metric--blue,
.home-page__scenario--signals {
  --home-page-accent: var(--color-blue-600);
}

.home-page__metric--green,
.home-page__scenario--hardware {
  --home-page-accent: var(--color-emerald-600);
}

.home-page__metric--amber,
.home-page__scenario--switchgears {
  --home-page-accent: var(--color-amber-500);
}

.home-page__metric--neutral,
.home-page__scenario--sequences {
  --home-page-accent: var(--color-neutral-500);
}

.home-page__workbench {
  display: grid;
  gap: 0.75rem;
}

.home-page__section-header {
  display: flex;
  align-items: end;
  justify-content: space-between;
  gap: 1rem;
  padding: 0.25rem 0.125rem 0;
}

.home-page__section-title {
  margin-top: 0.25rem;
  color: var(--color-neutral-950);
  font-size: var(--text-xl);
  font-weight: 800;
  letter-spacing: 0;
}

.home-page__quick-grid {
  display: grid;
  gap: 0.75rem;
}

.home-page__scenario {
  position: relative;
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  gap: 0.875rem;
  align-items: center;
  min-height: 7rem;
  overflow: hidden;
  padding: 1rem 1rem 1rem 1.25rem;
  color: var(--color-neutral-900);
  text-align: left;
  transition: border-color 150ms ease, box-shadow 150ms ease, transform 150ms ease;
}

.home-page__scenario:hover {
  border-color: color-mix(in srgb, var(--home-page-accent) 42%, var(--color-neutral-300));
  box-shadow:
    0 18px 34px rgb(15 23 42 / 0.11),
    inset 0 1px 0 rgb(255 255 255 / 86%);
  transform: translateY(-1px);
}

.home-page__scenario:focus-visible {
  outline: 2px solid var(--home-page-accent);
  outline-offset: 2px;
}

.home-page__scenario-index {
  display: inline-flex;
  width: 2.25rem;
  height: 2.25rem;
  align-items: center;
  justify-content: center;
  border: 1px solid color-mix(in srgb, var(--home-page-accent) 42%, var(--color-white));
  border-radius: var(--radius-lg);
  background: color-mix(in srgb, var(--home-page-accent) 12%, var(--color-white));
  color: var(--home-page-accent);
  font-size: var(--text-xs);
  font-weight: 800;
}

.home-page__scenario-content {
  display: grid;
  gap: 0.25rem;
  min-width: 0;
}

.home-page__scenario-title {
  color: var(--color-neutral-950);
  font-size: var(--text-sm);
  font-weight: 800;
}

.home-page__scenario-description {
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

.home-page__scenario-arrow {
  width: 1.25rem;
  height: 1.25rem;
  color: var(--home-page-accent);
}

:global(.dark .home-page) {
  background:
    linear-gradient(180deg, var(--color-neutral-950) 0%, var(--color-neutral-900) 48%, var(--color-neutral-950) 100%);
  color: var(--color-neutral-100);
}

:global(.dark .home-page::before) {
  background:
    radial-gradient(circle at 12% 20%, rgb(96 165 250 / 0.18) 0 1px, transparent 2px),
    radial-gradient(circle at 34% 34%, rgb(250 250 250 / 0.09) 0 1px, transparent 2px),
    radial-gradient(circle at 72% 22%, rgb(96 165 250 / 0.12) 0 1px, transparent 2px),
    linear-gradient(90deg, rgb(96 165 250 / 0.075) 1px, transparent 1px),
    linear-gradient(0deg, rgb(250 250 250 / 0.035) 1px, transparent 1px);
}

:global(.dark .home-page__header),
:global(.dark .home-page__workspace-panel),
:global(.dark .home-page__metric),
:global(.dark .home-page__scenario) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 88%, var(--color-neutral-950));
  box-shadow:
    0 16px 32px rgb(0 0 0 / 0.28),
    inset 0 1px 0 rgb(255 255 255 / 4%);
}

:global(.dark .home-page__workspace-title),
:global(.dark .home-page__metric-value),
:global(.dark .home-page__section-title),
:global(.dark .home-page__scenario-title) {
  color: var(--color-neutral-50);
}

:global(.dark .home-page__workspace-summary),
:global(.dark .home-page__scenario-description) {
  color: var(--color-neutral-300);
}

:global(.dark .home-page__eyebrow),
:global(.dark .home-page__metric-label),
:global(.dark .home-page__metric-detail),
:global(.dark .home-page__scenario-meta) {
  color: var(--color-neutral-400);
}

:global(.dark .home-page__signal-progress) {
  border-color: color-mix(in srgb, var(--color-blue-400) 28%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-blue-900) 22%, var(--color-neutral-900)), var(--color-neutral-900));
}

:global(.dark .home-page__signal-progress-header) {
  color: var(--color-neutral-100);
}

:global(.dark .home-page__signal-progress-header strong) {
  color: var(--color-sky-200);
}

:global(.dark .home-page__signal-progress-meter) {
  background: var(--color-neutral-800);
}

:global(.dark .home-page__signal-progress-meter span) {
  background: linear-gradient(90deg, var(--color-neutral-50), var(--color-blue-400));
}

:global(.dark .home-page__signal-progress p) {
  color: var(--color-neutral-300);
}

:global(.dark .home-page__scenario:hover) {
  border-color: color-mix(in srgb, var(--home-page-accent) 48%, var(--color-neutral-700));
  box-shadow:
    0 20px 38px rgb(0 0 0 / 0.36),
    inset 0 1px 0 rgb(255 255 255 / 6%);
}

:global(.dark .home-page__scenario-index) {
  border-color: color-mix(in srgb, var(--home-page-accent) 38%, var(--color-neutral-900));
  background: color-mix(in srgb, var(--home-page-accent) 18%, var(--color-neutral-900));
}

@media (min-width: 760px) {
  .home-page__shell {
    gap: 1.25rem;
    padding: 1.5rem;
  }

  .home-page__overview {
    grid-template-columns: minmax(0, 1.15fr) minmax(24rem, 0.85fr);
    align-items: stretch;
  }

  .home-page__quick-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (min-width: 1120px) {
  .home-page__shell {
    padding-block: 2rem;
  }
}

@media (max-width: 680px) {
  .home-page__header,
  .home-page__brand {
    align-items: flex-start;
    flex-direction: column;
  }

  .home-page__workspace-switcher {
    flex: 1 1 auto;
    width: auto;
    min-width: 0;
  }

  .home-page__header-actions {
    width: 100%;
  }

  .home-page__metrics {
    grid-template-columns: 1fr;
  }
}
</style>
