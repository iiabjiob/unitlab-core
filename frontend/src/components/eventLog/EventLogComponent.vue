<template>
  <div>
    hi Event log
  </div>
</template>

<script setup lang="ts">

import { useEventLogStore } from "@/stores/eventLogStore"
import UiButton from "../ui/UiButton.vue"



// ---- Store ----
const store = useEventLogStore()

</script>
