<template>
  <article class="device-card">
    <header class="device-card__header">
      <div class="device-card__title">
        <span class="device-card__unit">{{ device.name?.trim() || device.unit_id }}</span>
        <span class="device-card__type">{{ device.device_type.toUpperCase() }} · {{ device.num_channels }}ch</span>
      </div>
      <div class="device-card__meta">
        <span class="device-card__last">Last seen {{ lastSeenLabel }}</span>
        <OnlineStatusComponent :status="device.status" />
      </div>
    </header>

    <div class="device-card__body">
      <ChannelsComponent
        :unit-id="device.unit_id"
        :device-type="device.type"
        :num_channels="device.num_channels"
        :disabled="device.status !== 'online'"
      />
    </div>

    <footer v-if="device.location" class="device-card__footer">
      {{ device.location }}
    </footer>
  </article>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { Device } from "@/types/device"
import ChannelsComponent from "./ChannelsComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"

const props = defineProps<{
  device: Device
}>()

defineEmits<{
  (e: 'toggle', device: Device): void
  (e: 'delete', device: Device): void
}>()

const lastSeenLabel = computed(() => formatLastSeen(props.device.last_seen))

function formatLastSeen(ts?: number | null): string {
  if (!ts) {
    return "N/A"
  }
  const timestamp = typeof ts === "number" ? ts : Number(ts)
  if (!Number.isFinite(timestamp)) {
    return "N/A"
  }
  const ms = timestamp > 1e12 ? timestamp : timestamp * 1000
  const date = new Date(ms)
  if (Number.isNaN(date.getTime())) {
    return "N/A"
  }

  const pad = (value: number) => value.toString().padStart(2, "0")
  const time = `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`
  const day = pad(date.getDate())
  const month = pad(date.getMonth() + 1)
  const fullYear = date.getFullYear()
  return `${time} ${day}.${month}.${fullYear}`
}

</script>

<style scoped>
.device-card {
  background: #f8fafc;
  border: 1px solid rgba(148, 163, 184, 0.5);
  border-radius: 0.5rem;
  padding: 0.85rem 1rem;
  display: flex;
  flex-direction: column;
  gap: 0.65rem;
  box-shadow: none;
}

:global(.dark) .device-card {
  background: rgba(15, 23, 42, 0.85);
  border-color: rgba(71, 85, 105, 0.9);
}

.device-card__header {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  gap: 0.5rem;
}

.device-card__title {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  align-items: baseline;
}

.device-card__unit {
  font-family: "JetBrains Mono", "Fira Mono", monospace;
  font-size: 1rem;
  font-weight: 600;
  color: #0f172a;
}

:global(.dark) .device-card__unit {
  color: #e2e8f0;
}

.device-card__type {
  border: 1px solid rgba(148, 163, 184, 0.6);
  border-radius: 9999px;
  padding: 0.1rem 0.6rem;
  font-size: 0.7rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: #475569;
}

:global(.dark) .device-card__type {
  border-color: rgba(148, 163, 184, 0.6);
  color: #e2e8f0;
}

.device-card__meta {
  display: flex;
  align-items: center;
  gap: 0.6rem;
  font-size: 0.75rem;
  color: #64748b;
}

:global(.dark) .device-card__meta {
  color: #cbd5f5;
}

.device-card__last {
  white-space: nowrap;
}

.device-card__body {
  width: 100%;
  overflow-x: auto;
  padding-bottom: 0.25rem;
}

.device-card__body::-webkit-scrollbar {
  height: 6px;
}

.device-card__body::-webkit-scrollbar-track {
  background: transparent;
}

.device-card__body::-webkit-scrollbar-thumb {
  background: rgba(148, 163, 184, 0.5);
  border-radius: 9999px;
}

:global(.dark) .device-card__body::-webkit-scrollbar-thumb {
  background: rgba(71, 85, 105, 0.9);
}

.device-card__footer {
  font-size: 0.75rem;
  color: #64748b;
}

:global(.dark) .device-card__footer {
  color: #cbd5f5;
}
</style>
