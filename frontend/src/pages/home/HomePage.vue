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

        <div class="grid gap-4 md:grid-cols-3">
          <button
            v-for="card in statusCards"
            :key="card.label"
            type="button"
            class="rounded-2xl border border-neutral-200/70 bg-white/80 px-5 py-4 text-center transition hover:-translate-y-0.5 hover:border-neutral-900 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-neutral-900 dark:border-neutral-700 dark:bg-neutral-900/70 dark:hover:border-neutral-200"
            @click="goTo(card.route)"
          >
            <p class="text-[10px] uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">{{ card.label }}</p>
            <p class="mt-4 text-3xl font-semibold">{{ card.value }}</p>
            <p class="text-sm text-neutral-500 dark:text-neutral-400">{{ card.detail }}</p>
          </button>
        </div>

        <section class="rounded-2xl border border-neutral-200/70 bg-neutral-50/90 p-6 dark:border-neutral-800 dark:bg-neutral-900">
          <p class="text-xs uppercase tracking-[0.4em] text-neutral-500 dark:text-neutral-400">Quick actions</p>
          <div class="mt-5 grid gap-4 md:grid-cols-3">
            <button
              v-for="action in quickActions"
              :key="action.label"
              type="button"
              class="flex h-28 flex-col justify-between rounded-xl border border-neutral-200 bg-white px-4 py-3 text-left text-neutral-900 transition hover:-translate-y-0.5 hover:border-neutral-900 hover:bg-neutral-50 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-neutral-900 dark:border-neutral-700 dark:bg-neutral-950/40 dark:text-neutral-50 dark:hover:border-neutral-200/80 dark:hover:bg-neutral-900"
              @click="goTo(action.route)"
            >
              <span class="text-2xl">{{ action.icon }}</span>
              <div>
                <p class="text-base font-semibold">{{ action.label }}</p>
                <p v-if="action.caption" class="text-sm text-neutral-500 dark:text-neutral-400">{{ action.caption }}</p>
              </div>
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
import { useDeviceStore } from "@/stores/deviceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSequenceStore } from "@/stores/sequenceStore"

const workspaceStore = useWorkspaceStore()
const wsStore = useWebSocketStore()
const deviceStore = useDeviceStore()
const switchgearStore = useSwitchgearStore()
const sequenceStore = useSequenceStore()
const router = useRouter()

onMounted(() => {
  void workspaceStore.bootstrap()
  void deviceStore.ensureLoaded()
  void switchgearStore.ensureLoaded()
  void sequenceStore.ensureLoaded()
})

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

const statusCards = computed(() => {
  const onlineDevices = deviceStore.devices.filter(device => device.status === "online").length
  const totalDevices = deviceStore.devices.length
  const switchgears = switchgearStore.switchgears.length
  const sequences = sequenceStore.sequences.length

  return [
    {
      label: "Devices online",
      value: `${onlineDevices}/${totalDevices || 0}`,
      detail: totalDevices ? "Live modules" : "Waiting for devices",
      route: { name: "devices.list" },
    },
    {
      label: "Switchgears",
      value: switchgears,
      detail: switchgears ? "Configured cabinets" : "Add your first cabinet",
      route: { name: "switchgears.list" },
    },
    {
      label: "Sequences",
      value: sequences,
      detail: sequences ? "Ready test plans" : "No tests yet",
      route: { name: "sequences.list" },
    },
  ]
})

const quickActions = [
  {
    icon: "🔘",
    label: "Toggle channel",
    caption: "Jump to devices",
    route: { name: "devices.list" },
  },
  {
    icon: "🔁",
    label: "Simulate switch (ON / OFF)",
    caption: "Open switchgears",
    route: { name: "switchgears.list" },
  },
  {
    icon: "▶️",
    label: "Run full cabinet test",
    caption: "Launch sequences",
    route: { name: "sequences.list" },
  },
]

function goTo(route: { name: string }) {
  router.push(route)
}
</script>
