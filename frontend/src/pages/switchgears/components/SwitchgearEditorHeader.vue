<script setup lang="ts">
import { ref, computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import UiButton from "@/components/ui/UiButton.vue"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue";
import type { Switchgear } from "@/types/switchgear";

const props = defineProps<{
  switchgear: Switchgear
}>()

const store = useDeviceStore()

</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <!-- Switchgear name -->
      <div class="flex items-center gap-2">
        <div class="text-lg font-medium tracking-tight hover:text-blue-400 cursor-pointer">
          {{ switchgear.name }}
        </div>
      </div>

      <!-- Metadata -->
      <div class="text-xs text-neutral-500 leading-normal">
        <template v-if="switchgear">
          <div>{{ switchgear.switchgear_type }}</div>
        </template>
      </div>
    </div>
  </div>
</template>
