<template>
  <div class="home-page">
    <div class="home-page__viewport">
      <div class="home-page__panel">
        <div class="home-page__topbar">
          <AppLogo class="home-page__logo" />
          <OnlineStatusComponent :status="systemStatus" :description="systemStatusDescription" neutral-offline />
        </div>

        <div class="home-page__workspace">
          <div class="home-page__workspace-info">
            <p class="home-page__eyebrow">Workspace</p>
            <p class="home-page__workspace-summary">{{ workspaceSummary }}</p>
            <p class="home-page__workspace-metrics">
              {{ onlineDevicesCount }} devices online · {{ totalChannelsCount }} channels loaded
            </p>
            <p v-if="workspaceError" class="home-page__workspace-error">{{ workspaceError }}</p>
          </div>
          <div class="home-page__workspace-switcher">
            <WorkspaceSwitcher />
          </div>
        </div>

        <!-- <button
          v-if="recommendedAction"
          type="button"
          class="home-page__recommended"
          @click="goTo(recommendedAction.route)"
        >
          <div>
            <p class="home-page__recommended-label">Recommended Start</p>
            <p class="home-page__recommended-action">{{ recommendedAction.label }}</p>
          </div>
          <span class="home-page__recommended-arrow" aria-hidden="true">→</span>
        </button> -->

        <section class="home-page__quick">
          <div class="home-page__quick-header">
            <p class="home-page__eyebrow">Quick Actions</p>
            <p class="home-page__quick-hint">Choose what you need now</p>
          </div>

          <div class="home-page__quick-grid">
            <button
              v-for="scenario in scenarioCards"
              :key="scenario.title"
              type="button"
              class="home-page__scenario"
              @click="goTo(scenario.route)"
            >
              <div class="home-page__scenario-body">
                <div class="home-page__scenario-heading">
                  <span class="home-page__scenario-icon" aria-hidden="true">{{ scenario.emoji }}</span>
                  <h3 class="home-page__scenario-title">{{ scenario.title }}</h3>
                </div>
                <p class="home-page__scenario-description">{{ scenario.description }}</p>
              </div>
              <span class="home-page__scenario-cta">
                {{ scenario.cta }}
                <span aria-hidden="true">↗</span>
              </span>
            </button>
          </div>
        </section>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, watch } from "vue"
import { useRouter, type RouteLocationRaw } from "vue-router"
import AppLogo from "@/components/layout/AppLogo.vue"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"
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

const systemStatus = computed(() => systemHealthStore.status)
const systemStatusDescription = computed(() => (
  systemStatus.value === "degraded" ? systemHealthStore.tooltip : null
))
const onlineDevicesCount = computed(() => deviceStore.devices.filter((device) => device.status === "online").length)
const totalChannelsCount = computed(() => channelStore.channels.length)

const workspaceSummary = computed(() => {
  if (workspaceStore.loading) {
    return "Syncing workspaces…"
  }

  if (!workspaceStore.workspaces.length) {
    return "Preparing your default workspace…"
  }

  if (workspaceStore.workspaces.length === 1) {
    const label = workspaceStore.activeWorkspace?.name ?? workspaceStore.workspaces[0].name
    return `${label} is ready.`
  }

  return `${workspaceStore.workspaces.length} workspaces are ready.`
})

const workspaceError = computed(() => workspaceStore.error)

type HomeRoute = RouteLocationRaw
type HomeAction = { label: string; route: HomeRoute }
type QuickAction = {
  title: string
  description: string
  cta: string
  emoji: string
  route: HomeRoute
}

const recommendedAction = computed<HomeAction>(() => {
  if (signalSheetStore.hasSheet) {
    return {
      label: "Start controlling signals",
      route: { name: "signals.home" },
    }
  }

  return {
    label: "Import signal list from project",
    route: { name: "signals.home", query: { import: "1" } },
  }
})

const scenarioCards: QuickAction[] = [
  {
    title: "Live Hardware",
    description: "Direct control of physical I/O channels.",
    cta: "Open",
    emoji: "",
    route: { name: "devices.list" },
  },
  {
    title: "Signals",
    description: "Project signal names with fast channel mapping.",
    cta: "Open",
    emoji: "",
    route: { name: "signals.home" },
  },
  {
    title: "Switchgears",
    description: "Operate and validate disconnector feedback.",
    cta: "Open",
    emoji: "",
    route: { name: "switchgears.list" },
  },
  {
    title: "Sequencer",
    description: "Build and run repeatable test instructions.",
    cta: "Open",
    emoji: "",
    route: { name: "instructions.list" },
  },
]

watch(
  () => workspaceStore.activeWorkspaceId,
  (workspaceId) => {
    if (!workspaceId) {
      return
    }
    const needsRefresh = (
      !signalSheetStore.loadingSheet &&
      (
        signalSheetStore.lastSheetLoadedAt === null ||
        signalSheetStore.sheet?.workspace_id !== workspaceId
      )
    )
    if (needsRefresh) {
      void runStoreBootstrap(
        ["home-signal-sheet", workspaceId],
        [() => signalSheetStore.refreshSheet()],
        { mode: "settled" },
      )
    }
  },
  { immediate: true },
)

function goTo(route: HomeRoute) {
  router.push(route)
}
</script>

<style scoped>
.home-page {
  min-height: 100vh;
  min-height: 100svh;
  background: linear-gradient(
    to bottom,
    var(--color-neutral-50),
    var(--color-white),
    var(--color-neutral-100)
  );
  color: var(--color-neutral-900);
}

.home-page__viewport {
  display: flex;
  width: 100%;
  min-height: 100vh;
  min-height: 100svh;
  max-width: 72rem;
  align-items: flex-start;
  justify-content: center;
  margin: 0 auto;
  padding: 1rem;
}

.home-page__panel {
  display: grid;
  width: 100%;
  max-width: 64rem;
  grid-template-rows: auto auto auto auto;
  gap: 1rem;
  padding: 1.25rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1.5rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  box-shadow: 0 25px 60px rgb(15 23 42 / 0.15);
  backdrop-filter: blur(8px);
}

.home-page__topbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 1rem;
}

.home-page__logo :deep(.app-logo__link) {
  color: var(--color-neutral-900);
  font-size: var(--text-3xl);
  font-weight: 600;
  letter-spacing: 0;
}

.home-page__workspace {
  display: grid;
  gap: 0.75rem;
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: 1rem;
  background: var(--color-white);
}

.home-page__workspace-info {
  min-width: 0;
}

.home-page__eyebrow {
  margin: 0;
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 400;
  letter-spacing: 0.26em;
  text-transform: uppercase;
}

.home-page__workspace-summary {
  margin: 0.25rem 0 0;
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
}

.home-page__workspace-metrics {
  margin: 0.25rem 0 0;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.home-page__workspace-error {
  margin: 0.25rem 0 0;
  color: var(--color-red-500);
  font-size: var(--text-sm);
}

.home-page__workspace-switcher {
  width: 100%;
}

.home-page__recommended {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  width: 100%;
  padding: 0.75rem 1rem;
  border: 1px solid color-mix(in srgb, var(--color-emerald-300) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-emerald-50) 70%, transparent);
  text-align: left;
  transition: background 150ms ease, border-color 150ms ease;
}

.home-page__recommended:hover {
  border-color: var(--color-emerald-500);
  background: var(--color-emerald-50);
}

.home-page__recommended:focus-visible,
.home-page__scenario:focus-visible {
  outline: 2px solid currentColor;
  outline-offset: 2px;
}

.home-page__recommended-label {
  margin: 0;
  color: var(--color-emerald-700);
  font-size: 0.6875rem;
  letter-spacing: 0.26em;
  text-transform: uppercase;
}

.home-page__recommended-action {
  margin: 0.25rem 0 0;
  color: var(--color-emerald-900);
  font-size: var(--text-sm);
  font-weight: 600;
}

.home-page__recommended-arrow {
  color: var(--color-emerald-700);
  font-size: var(--text-base);
}

.home-page__quick {
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-neutral-50) 90%, transparent);
}

.home-page__quick-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
  margin-bottom: 0.75rem;
}

.home-page__quick-hint {
  margin: 0;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.home-page__quick-grid {
  display: grid;
  gap: 0.75rem;
}

.home-page__scenario {
  display: flex;
  height: 7rem;
  min-height: 0;
  flex-direction: column;
  justify-content: space-between;
  padding: 0.75rem 1rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  color: var(--color-neutral-900);
  text-align: left;
  transition: background 150ms ease, border-color 150ms ease;
}

.home-page__scenario:hover {
  border-color: var(--color-neutral-300);
  background: var(--color-white);
}

.home-page__scenario-body {
  display: grid;
  gap: 0.25rem;
}

.home-page__scenario-heading {
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.home-page__scenario-icon {
  font-size: 2.25rem;
  line-height: 1;
}

.home-page__scenario-title {
  margin: 0;
  font-size: var(--text-sm);
  font-weight: 600;
  line-height: 1.25;
}

.home-page__scenario-description {
  margin: 0;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.home-page__scenario-cta {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
  margin-top: 0.5rem;
  color: var(--color-emerald-600);
  font-size: var(--text-xs);
  font-weight: 600;
  text-decoration-color: currentColor;
  text-underline-offset: 2px;
}

.home-page__scenario:hover .home-page__scenario-cta {
  text-decoration-line: underline;
  text-decoration-skip-ink: none;
}

:global(.dark .home-page) {
  background: linear-gradient(
    to bottom,
    var(--color-neutral-950),
    var(--color-neutral-900),
    var(--color-neutral-900)
  );
  color: var(--color-neutral-50);
}

:global(.dark .home-page__panel) {
  border-color: color-mix(in srgb, var(--color-neutral-800) 80%, transparent);
  background: color-mix(in srgb, var(--color-neutral-900) 80%, transparent);
}

:global(.dark .home-page__logo .app-logo__link) {
  color: var(--color-neutral-50);
}

:global(.dark .home-page__workspace) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
}

:global(.dark .home-page__eyebrow),
:global(.dark .home-page__workspace-metrics),
:global(.dark .home-page__quick-hint),
:global(.dark .home-page__scenario-description) {
  color: var(--color-neutral-400);
}

:global(.dark .home-page__workspace-summary) {
  color: var(--color-neutral-300);
}

:global(.dark .home-page__recommended) {
  border-color: color-mix(in srgb, var(--color-emerald-700) 80%, transparent);
  background: color-mix(in srgb, var(--color-emerald-900) 35%, transparent);
}

:global(.dark .home-page__recommended:hover) {
  border-color: var(--color-emerald-400);
  background: color-mix(in srgb, var(--color-emerald-900) 40%, transparent);
}

:global(.dark .home-page__recommended-label),
:global(.dark .home-page__recommended-arrow),
:global(.dark .home-page__scenario-cta) {
  color: var(--color-emerald-300);
}

:global(.dark .home-page__recommended-action) {
  color: color-mix(in srgb, var(--color-emerald-300) 75%, var(--color-white));
}

:global(.dark .home-page__quick) {
  border-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}

:global(.dark .home-page__scenario) {
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
  color: var(--color-neutral-50);
}

:global(.dark .home-page__scenario:hover) {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-900);
}

@media (min-width: 640px) {
  .home-page__viewport {
    align-items: center;
    padding: 1.5rem;
  }

  .home-page__panel {
    padding: 1.5rem;
  }
}

@media (min-width: 768px) {
  .home-page__workspace {
    grid-template-columns: minmax(0, 1fr) auto;
    align-items: center;
  }

  .home-page__workspace-switcher {
    width: 20rem;
  }

  .home-page__quick-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}
</style>
