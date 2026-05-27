<template>
  <div class="time-component">
    <span class="time-component__clock">{{ formattedTime }}</span>
    <component
      :is="ntpBadgeInteractive ? 'button' : 'span'"
      :type="ntpBadgeInteractive ? 'button' : undefined"
      class="time-component__ntp-badge"
      :class="[
        coreNtpStore.isSynced ? 'time-component__ntp-badge--synced' : 'time-component__ntp-badge--unsynced',
        ntpBadgeInteractive ? 'time-component__ntp-badge--interactive' : '',
      ]"
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
    return "SYNC"
  }
  return "UNSYNC"
})

const ntpTitle = computed(() => {
  if (coreNtpStore.isSynced) {
    const source = String(coreNtpStore.selectedUpstreamSource?.name ?? coreNtpStore.tracking?.source ?? "").trim()
    return source
      ? `RPi5 synchronized with upstream NTP source ${source}`
      : "RPi5 synchronized with an upstream NTP source"
  }
  return "RPi5 is not synchronized with an upstream precise time source. It still serves NTP to peripherals. Click to open Time / NTP settings."
})

const ntpBadgeInteractive = computed(() => !coreNtpStore.isSynced)

function handleNtpBadgeClick() {
  if (!ntpBadgeInteractive.value) {
    return
  }
  void router.push({ name: "settings.ntp" }).catch(() => undefined)
}
</script>

<style scoped>
.time-component {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  column-gap: 0.5rem;
  padding-block: 0.125rem;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

.time-component__clock {
  font-family: var(--font-mono);
  font-variant-numeric: tabular-nums;
}

.time-component__ntp-badge {
  display: inline-flex;
  align-items: center;
  padding: 0.125rem 0.375rem;
  border: 1px solid;
  border-radius: var(--radius-sm);
  font-size: 10px;
  font-weight: 600;
  letter-spacing: 0;
  line-height: 1.2;
  text-transform: uppercase;
}

button.time-component__ntp-badge {
  appearance: none;
  font-family: inherit;
}

.time-component__ntp-badge--interactive:focus-visible {
  outline: 2px solid var(--color-amber-500);
  outline-offset: 2px;
}

.time-component__ntp-badge--synced {
  border-color: var(--color-emerald-300);
  background: var(--color-emerald-50);
  color: var(--color-emerald-700);
}

.time-component__ntp-badge--unsynced {
  border-color: var(--color-amber-300);
  background: var(--color-amber-50);
  color: var(--color-amber-700);
}

:global(.dark .time-component){
  color: var(--color-neutral-400);
}

:global(.dark .time-component__ntp-badge--synced){
  border-color: color-mix(in srgb, var(--color-emerald-700) 70%, transparent);
  background: color-mix(in srgb, var(--color-emerald-900) 40%, transparent);
  color: var(--color-emerald-300);
}

:global(.dark .time-component__ntp-badge--unsynced){
  border-color: color-mix(in srgb, var(--color-amber-700) 70%, transparent);
  background: color-mix(in srgb, var(--color-amber-900) 40%, transparent);
  color: var(--color-amber-300);
}
</style>
