<template>
  <div class="h-dvh overflow-hidden bg-gradient-to-b from-neutral-50 via-white to-neutral-100 text-neutral-900 dark:from-neutral-950 dark:via-neutral-900 dark:to-neutral-900 dark:text-neutral-50">
    <div class="mx-auto flex h-full w-full max-w-6xl items-center justify-center px-4 py-4 sm:px-6 sm:py-6">
      <div class="grid w-full max-w-5xl grid-rows-[auto_auto_auto_auto] gap-4 rounded-3xl border border-neutral-200/70 bg-white/90 p-5 shadow-[0_25px_60px_rgba(15,23,42,0.15)] backdrop-blur dark:border-neutral-800/80 dark:bg-neutral-900/80 sm:p-6">
        <div class="flex items-center justify-between gap-4">
          <AppLogo class="text-3xl font-semibold tracking-tight text-neutral-900 dark:text-neutral-50" />
          <OnlineStatusComponent :status="systemStatus" :description="systemStatusDescription" neutral-offline />
        </div>

        <div class="grid gap-3 rounded-2xl border border-neutral-200/80 bg-white p-4 dark:border-neutral-800 dark:bg-neutral-950/40 md:grid-cols-[minmax(0,1fr)_auto] md:items-center">
          <div class="min-w-0">
            <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Workspace</p>
            <p class="mt-1 text-sm text-neutral-600 dark:text-neutral-300">{{ workspaceSummary }}</p>
            <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
              {{ onlineDevicesCount }} devices online · {{ totalChannelsCount }} channels loaded
            </p>
            <p v-if="workspaceError" class="mt-1 text-sm text-red-500">{{ workspaceError }}</p>
          </div>
          <div class="w-full md:w-[320px]">
            <WorkspaceSwitcher />
          </div>
        </div>

        <button
          v-if="recommendedAction"
          type="button"
          class="flex items-center justify-between gap-3 rounded-2xl border border-emerald-300/70 bg-emerald-50/70 px-4 py-3 text-left transition hover:border-emerald-500 hover:bg-emerald-50 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-emerald-600 dark:border-emerald-700/80 dark:bg-emerald-950/35 dark:hover:border-emerald-400 dark:hover:bg-emerald-900/40"
          @click="goTo(recommendedAction.route)"
        >
          <div>
            <p class="text-[11px] uppercase tracking-[0.26em] text-emerald-700 dark:text-emerald-300">Recommended Start</p>
            <p class="mt-1 text-sm font-semibold text-emerald-900 dark:text-emerald-200">{{ recommendedAction.label }}</p>
          </div>
          <span class="text-base text-emerald-700 dark:text-emerald-300" aria-hidden="true">→</span>
        </button>

        <section class="rounded-2xl border border-neutral-200/70 bg-neutral-50/90 p-4 dark:border-neutral-800 dark:bg-neutral-900">
          <div class="mb-3 flex items-center justify-between gap-2">
            <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Quick Actions</p>
            <p class="text-xs text-neutral-500 dark:text-neutral-400">Choose what you need now</p>
          </div>

          <div class="grid gap-3 md:grid-cols-2">
            <button
              v-for="scenario in scenarioCards"
              :key="scenario.title"
              type="button"
              class="flex h-28 min-h-0 flex-col justify-between rounded-2xl border border-neutral-200 bg-white/90 px-4 py-3 text-left text-neutral-900 transition hover:-translate-y-0.5 hover:border-neutral-900 hover:bg-white focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-neutral-900 dark:border-neutral-700 dark:bg-neutral-950/40 dark:text-neutral-50 dark:hover:border-neutral-200/80 dark:hover:bg-neutral-900"
              @click="goTo(scenario.route)"
            >
              <div class="space-y-1">
                <div class="flex items-center gap-2">
                  <span class="inline-flex h-6 w-6 items-center justify-center rounded-md bg-neutral-100 text-neutral-600 dark:bg-neutral-800 dark:text-neutral-300">
                    <svg viewBox="0 0 24 24" class="h-4 w-4" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">
                      <path :d="scenario.iconPath" stroke-linecap="round" stroke-linejoin="round" />
                    </svg>
                  </span>
                  <h3 class="text-sm font-semibold leading-tight">{{ scenario.title }}</h3>
                </div>
                <p class="text-xs text-neutral-500 dark:text-neutral-400">{{ scenario.description }}</p>
              </div>
              <span class="mt-2 inline-flex items-center gap-1 text-xs font-semibold text-emerald-600 dark:text-emerald-300">
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
import { computed } from "vue"
import { useRouter, type RouteLocationRaw } from "vue-router"
import AppLogo from "@/components/layout/AppLogo.vue"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"

const workspaceStore = useWorkspaceStore()
const systemHealthStore = useSystemHealthStore()
const deviceStore = useDeviceStore()
const channelStore = useChannelStore()
const router = useRouter()

const systemStatus = computed(() => systemHealthStore.status)
const systemStatusDescription = computed(() => systemHealthStore.tooltip)
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
  iconPath: string
  route: HomeRoute
}

const recommendedAction: HomeAction = {
  label: "Import signal list from project",
  route: { name: "signals.home", query: { import: "1" } },
}

const scenarioCards: QuickAction[] = [
  {
    title: "Live Hardware",
    description: "Direct control of physical I/O channels.",
    cta: "Open",
    iconPath: "M3 10h18M6 14h12M9 18h6M5 6l2-2h10l2 2",
    route: { name: "devices.list" },
  },
  {
    title: "Signals",
    description: "Project signal names with fast channel mapping.",
    cta: "Open",
    iconPath: "M4 17h3l3-5 3 4 4-8 3 2",
    route: { name: "signals.home" },
  },
  {
    title: "Switchgears",
    description: "Operate and validate disconnector feedback.",
    cta: "Open",
    iconPath: "M12 2v8m0 0 3-3m-3 3-3-3m3 8v7m0 0 3-3m-3 3-3-3",
    route: { name: "switchgears.list" },
  },
  {
    title: "Sequencer",
    description: "Build and run repeatable test instructions.",
    cta: "Open",
    iconPath: "M6 7h12M6 12h12M6 17h8M4 7h.01M4 12h.01M4 17h.01",
    route: { name: "instructions.list" },
  },
]

function goTo(route: HomeRoute) {
  router.push(route)
}
</script>
