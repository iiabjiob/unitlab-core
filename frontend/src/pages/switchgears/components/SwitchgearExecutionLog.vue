<script setup lang="ts">
import { computed } from "vue"
import ExecutionLogPanel from "@/components/ui/ExecutionLogPanel.vue"
import type { Switchgear } from "@/types/switchgear"
import { useSwitchgearLogStore } from "@/stores/switchgearLogStore"

const props = defineProps<{
  switchgear: Switchgear
}>()

const logStore = useSwitchgearLogStore()
const sortedLogs = computed(() =>
  [...(logStore.logs[props.switchgear.id] ?? [])]
    .sort((a, b) => a.t - b.t)
)
</script>

<template>
  <ExecutionLogPanel :logs="sortedLogs" />
</template>
