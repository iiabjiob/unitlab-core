<script setup lang="ts">
import { ref, computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useRouter, useRoute } from "vue-router"
import SequenceListItem from "./SequenceListItem.vue"
import UiButton from "@/components/ui/UiButton.vue"

const store = useSequenceStore()
const router = useRouter()
const route = useRoute()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openSequence(id: number) {
  router.push(`/sequences/${id}`)
}

function addSequence() {
  store.createSequenceAuto().then(seq => {
    router.push(`/sequences/${seq.id}`)
  })
}

// SEARCH
const query = ref("")

const filteredSequences = computed(() => {
  if (!query.value.trim()) return store.sequences

  const q = query.value.toLowerCase()

  return store.sequences.filter(s =>
    s.name.toLowerCase().includes(q) ||
    (s.description && s.description.toLowerCase().includes(q))
  )
})
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <div class="mb-3">
      <UiButton
        variant="primary"
        size="sm"
        full
        @click="addSequence"
      >
        + New Sequence
      </UiButton>
    </div>

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        name="sequence-search"
        placeholder="Search sequences…"
        class="w-full px-3 py-1 text-sm rounded border border-gray-700
               text-gray-800 dark:text-gray-200 placeholder-gray-500"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-for="seq in filteredSequences"
        :key="seq.id"
      >
        <SequenceListItem
          :sequence="seq"
          :active="isActive(seq.id)"
          @click="openSequence(seq.id)"
        />
      </div>

      <div
        v-if="filteredSequences.length === 0"
        class="text-gray-500 text-xs italic px-2 py-2"
      >
        No sequences found
      </div>
    </div>

  </div>
</template>
