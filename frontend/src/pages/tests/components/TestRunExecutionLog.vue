<script setup lang="ts">
import { computed } from "vue"
import ExecutionLogPanel from "@/components/ui/ExecutionLogPanel.vue"
import type { TestRunState, TestRunSummary } from "@/types/testRuns"
import { useTestRunLogStore } from "@/stores/testRunLogStore"

const props = defineProps<{ run: TestRunSummary; state?: TestRunState | null }>()

const logStore = useTestRunLogStore()
const logs = computed(() => logStore.logs[props.run.id] ?? [])
</script>

<template>
  <ExecutionLogPanel :logs="logs" />
</template>
