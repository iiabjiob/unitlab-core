<template>
  <div
    class="flex flex-wrap items-center text-xs gap-x-2 py-0.5 text-neutral-600 dark:text-neutral-400">
    <span class="tabular-nums font-mono">{{ formattedTime }}</span>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from "vue"

const now = ref<Date | null>(null)
let intervalId: number | null = null

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
})
const formattedTime = computed(() => (now.value ? formatter.format(now.value) : "—"))
</script>
