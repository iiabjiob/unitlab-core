<script setup lang="ts">
import WorkspacePlaceholder from "@/components/ui/WorkspacePlaceholder.vue"
import { RouterLink } from "vue-router"
</script>

<template>
    <WorkspacePlaceholder
      tag="Test Runs"
      title="Select a test run"
      description="Choose an existing run from the sidebar or start a new execution when you're ready."
    />
</template>
