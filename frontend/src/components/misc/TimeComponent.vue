<template>
  <div
    class="flex flex-wrap items-center text-xs gap-x-2 py-0.5 text-neutral-600 dark:text-neutral-400">
    <span class="tabular-nums font-mono">{{ formattedTime }}</span>
    <component
      :is="ntpBadgeInteractive ? 'button' : 'span'"
      :type="ntpBadgeInteractive ? 'button' : undefined"
      class="inline-flex items-center rounded border px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-wide"
      :class="[ntpBadgeClass, 'cursor-default', ntpBadgeInteractive ? 'focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-amber-500' : '']"
      :title="ntpTitle"
      :aria-label="ntpTitle"
      @click="handleNtpBadgeClick"
    >
      {{ ntpLabel }}
    </component>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from "vue"
import { useRouter } from "vue-router"
import { useCoreNtpStore } from "@/stores/coreNtpStore"

const now = ref<Date | null>(null)
let intervalId: number | null = null
const coreNtpStore = useCoreNtpStore()
const router = useRouter()

const formatter = new Intl.DateTimeFormat(undefined, {
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
  hour12: false,
})

const tick = () => {
  now.value = new Date()
}
onMounted(() => {
  tick()
  intervalId = window.setInterval(tick, 1000)
  coreNtpStore.startMonitoring()
})

onUnmounted(() => {
  if (intervalId !== null) {
    window.clearInterval(intervalId)
    intervalId = null
  }
  coreNtpStore.stopMonitoring()
})

const formattedTime = computed(() => (now.value ? formatter.format(now.value) : "—"))

const ntpLabel = computed(() => {
  if (coreNtpStore.isSynced) {
    return "NTP"
  }
  return "UNSYNC"
})

const ntpTitle = computed(() => {
  if (coreNtpStore.isSynced) {
    const source = String(coreNtpStore.tracking?.source ?? "").trim()
    return source ? `Time synchronized via ${source}` : "Time synchronized via NTP"
  }
  return "Time is not synchronized. Click to open Time / NTP settings"
})

const ntpBadgeInteractive = computed(() => !coreNtpStore.isSynced)

function handleNtpBadgeClick() {
  if (!ntpBadgeInteractive.value) {
    return
  }
  void router.push({ name: "settings.ntp" }).catch(() => undefined)
}

const ntpBadgeClass = computed(() => {
  if (coreNtpStore.isSynced) {
    return "border-emerald-300 bg-emerald-50 text-emerald-700 dark:border-emerald-700/70 dark:bg-emerald-900/40 dark:text-emerald-300"
  }
  return "border-amber-300 bg-amber-50 text-amber-700 dark:border-amber-700/70 dark:bg-amber-900/40 dark:text-amber-300"
})
</script>
