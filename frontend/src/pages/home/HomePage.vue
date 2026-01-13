<template>
  <div class="bg-gradient-to-b from-neutral-50 via-white to-neutral-100 text-neutral-900 dark:from-neutral-950 dark:via-neutral-900 dark:to-neutral-900 dark:text-neutral-50">
    <div class="mx-auto flex min-h-dvh w-full max-w-5xl flex-col items-center justify-center px-6 py-16">
      <div class="w-full space-y-10 rounded-3xl border border-neutral-200/70 bg-white/90 p-8 shadow-[0_25px_60px_rgba(15,23,42,0.15)] backdrop-blur dark:border-neutral-800/80 dark:bg-neutral-900/80">
        <div class="flex flex-col items-center gap-5 text-center">
          <AppLogo class="text-4xl font-semibold tracking-tight text-neutral-900 dark:text-neutral-50" />
          <span
            class="inline-flex items-center gap-2 rounded-full px-4 py-1 text-sm font-semibold ring-1 ring-inset"
            :class="connectionState.chipClass"
          >
            <span class="h-2 w-2 rounded-full" :class="connectionState.dotClass"></span>
            {{ connectionState.label }}
          </span>
        </div>

        <div class="flex justify-center">
          <section class="w-full max-w-3xl rounded-2xl border border-neutral-200/80 bg-white p-6 dark:border-neutral-800 dark:bg-neutral-950/40">
            <p class="text-xs uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">Workspace</p>
            <div class="mt-4 space-y-4">
              <WorkspaceSwitcher />
              <p class="text-sm text-neutral-500 dark:text-neutral-400">{{ workspaceSummary }}</p>
              <p v-if="workspaceError" class="text-sm text-red-500">{{ workspaceError }}</p>
            </div>
          </section>
        </div>

        <section class="rounded-2xl border border-neutral-200/70 bg-neutral-50/90 p-6 dark:border-neutral-800 dark:bg-neutral-900">
          <div class="flex flex-col gap-2 text-left sm:flex-row sm:items-center sm:justify-between">
            <p class="text-xs uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">Choose your next action</p>
            <p class="text-sm text-neutral-500 dark:text-neutral-400">Pick the scenario that matches the job in front of you.</p>
          </div>

          <div class="mt-6 grid gap-4 md:grid-cols-2">
            <button
              v-for="scenario in scenarioCards"
              :key="scenario.title"
              type="button"
              class="flex flex-col justify-between rounded-2xl border border-neutral-200 bg-white/90 p-5 text-left text-neutral-900 transition hover:-translate-y-0.5 hover:border-neutral-900 hover:bg-white focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-neutral-900 dark:border-neutral-700 dark:bg-neutral-950/40 dark:text-neutral-50 dark:hover:border-neutral-200/80 dark:hover:bg-neutral-900"
              @click="goTo(scenario.route)"
            >
              <div class="flex items-center justify-between gap-3">
                <p class="text-[10px] uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">{{ scenario.badge }}</p>
                <span aria-hidden="true" class="text-lg text-neutral-400 dark:text-neutral-500">→</span>
              </div>
              <div class="mt-4 space-y-2">
                <h3 class="text-xl font-semibold leading-tight">{{ scenario.title }}</h3>
                <p class="text-sm text-neutral-500 dark:text-neutral-400">{{ scenario.description }}</p>
              </div>
              <span class="mt-6 inline-flex items-center gap-1 text-sm font-semibold text-emerald-600 dark:text-emerald-300">
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
import { computed, onMounted } from "vue"
import { useRouter } from "vue-router"
import AppLogo from "@/components/layout/AppLogo.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useWebSocketStore } from "@/stores/websocketStore"

const workspaceStore = useWorkspaceStore()
const wsStore = useWebSocketStore()
const router = useRouter()


const connectionState = computed(() => {
  if (wsStore.isConnected) {
    return {
      label: "Connected",
      chipClass: "bg-emerald-100/80 text-emerald-700 ring-emerald-500/40 dark:bg-emerald-500/20 dark:text-emerald-200",
      dotClass: "bg-emerald-500 animate-pulse",
    }
  }

  if (wsStore.everConnected) {
    return {
      label: "Reconnecting…",
      chipClass: "bg-amber-100/80 text-amber-700 ring-amber-400/40 dark:bg-amber-500/20 dark:text-amber-200",
      dotClass: "bg-amber-400 animate-pulse",
    }
  }

  return {
    label: "Connecting…",
    chipClass: "bg-neutral-200 text-neutral-700 ring-neutral-400/40 dark:bg-neutral-800 dark:text-neutral-200",
    dotClass: "bg-neutral-400 animate-pulse",
  }
})

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

const scenarioCards = [
  {
    badge: "Scenario 01",
    title: "Live Hardware Control",
    description: "Use when you need instant manual access to raw I/O channels without loading a project context.",
    cta: "Open Live Hardware",
    route: { name: "devices.list" },
  },
  {
    badge: "Scenario 02",
    title: "Live Signal Control",
    description: "Use when you prefer human-readable project signal names for guided manual intervention.",
    cta: "Go to Signals",
    route: { name: "signals.home" },
  },
  {
    badge: "Scenario 03",
    title: "Switchgear / Disconnectors",
    description: "Use when you must operate two-position disconnectors and verify feedback before energizing anything else.",
    cta: "Open Switchgears",
    route: { name: "switchgears.list" },
  },
  {
    badge: "Scenario 04",
    title: "Test Runs",
    description: "Use when you are ready to execute a scripted FAT batch with automatic logging and safety prompts.",
    cta: "Review Test Runs",
    route: { name: "testRuns.home" },
  },
]

function goTo(route: { name: string }) {
  router.push(route)
}
</script>
