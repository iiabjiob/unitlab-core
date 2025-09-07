<template>
  <div class="p-5">

    <ul class="grid gap-4 grid-cols-1 md:grid-cols-2">
      <SequenceCard
        v-for="seq in sequences"
        :key="seq.id"
        :sequence="seq"
      />
    </ul>
  </div>
</template>

<script setup lang="ts">
import SequenceCard from "@/components/sequences/SequenceCard.vue"
import { buildPulseDemoSequence } from "@/sequences/demoPulseSequences"
import { buildPilotSequence } from "@/sequences/demoSequences"

const sequences = [
  buildPulseDemoSequence(),
  buildPilotSequence()
]
</script>
