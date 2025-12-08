<script setup lang="ts">
import { computed } from "vue"
import { useRoute } from "vue-router"

import { useSequenceStore } from "@/stores/sequenceStore"

import SequenceEditorHeader from "./components/SequenceEditorHeader.vue"
import SequenceRunControls from "./components/SequenceRunControls.vue"
import SequenceStepsList from "./components/SequenceStepsList.vue"
import SequenceExecutionLog from "./components/SequenceExecutionLog.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"

const route = useRoute()
const store = useSequenceStore()

const sequenceId = computed(() => Number(route.params.id))

const sequence = computed(() =>
  store.sequences.find(s => s.id === sequenceId.value)
)

const state = computed(() =>
  store.states[sequenceId.value]
)
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <SequenceEditorHeader
      v-if="sequence"
      :sequence="sequence"
    />

    <!-- RUN CONTROLS -->
    <SequenceRunControls
      v-if="sequence && state"
      :sequence="sequence"
      :state="state"
      @start="store.startSequence(sequenceId)"
      @stop="store.stopSequence(sequenceId)"
    />

    <div class="flex flex-1 overflow-hidden bg-white dark:bg-neutral-800 shadow rounded p-5">

      <!-- STEP LIST -->
      <ResizablePanel
        v-if="sequence" :sequence="sequence"
        placement="left"
        storageKey="sequence-steps-list-width"
        :minSize="380"
        :defaultSize="380"
        :maxSize="800"
        >
        <SequenceStepsList :sequence="sequence" />
      </ResizablePanel>
      

      <!-- LOG PANEL -->
      <div v-if="sequence && state" class="flex-1 overflow-y-auto" >
        <SequenceExecutionLog :sequence="sequence" :state="state" />
      </div>

    </div>

  </div>
</template>
