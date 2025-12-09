<script setup lang="ts">
import { computed } from "vue"
import { useRoute } from "vue-router"

import { useSwitchgearStore } from "@/stores/switchgearStore"

import SwitchgearEditorHeader from "./components/SwitchgearEditorHeader.vue"
import SwitchgearExecutionLog from "./components/SwitchgearExecutionLog.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"

const route = useRoute()
const store = useSwitchgearStore()

const switchgearId = computed(() => Number(route.params.id))

const switchgear = computed(() =>
  store.switchgears.find(s => s.id === switchgearId.value)
)

</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <SwitchgearEditorHeader
      v-if="switchgear"
      :switchgear="switchgear"
    />

    <div class="flex flex-1 overflow-hidden bg-white dark:bg-neutral-800 shadow rounded p-5 mt-5">

      <!-- EDIT SWITCHGEAR -->
      <ResizablePanel
        v-if="switchgear" :switchgear="switchgear"
        placement="left"
        storageKey="switchgear-list-width"
        :minSize="380"
        :defaultSize="380"
        :maxSize="800"
        >
        TODO: Switchgear edit form
      </ResizablePanel>
      

      <!-- LOG PANEL -->
      <div v-if="switchgear" class="flex-1 overflow-y-auto" >
        <SwitchgearExecutionLog :switchgear="switchgear"/>
      </div>

    </div>

  </div>
</template>
