<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <!-- Header with burger + status -->
    <MobileHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <div class="flex gap-3 items-center">
          <AppLogo />
          <OnlineStatusComponent :status="status" />
        </div>
      </template>
      <template #right>
        <TimeComponent />
      </template>
    </MobileHeader>

    <!-- Header bulk actions -->
    <div v-if="meta.headerBulkActions" class="shrink-0">
      <slot name="header-bulk-actions" />
    </div>

    <!-- Main content -->
    <main class="flex-1 overflow-auto relative">
      <RouterView />
    </main>

    <!-- Event log fixed at the bottom -->
    <div
      v-if="meta.globalEventLog"
      class="h-40 overflow-y-auto border-t border-neutral-200 dark:border-neutral-700"
    >
      <EventLog />
    </div>

    <!-- Bottom validator -->
    <BottomValidatorResizable v-if="meta.bottomValidator" :errors="3" :warnings="2">
      <BottomValidator
        :messages="[
          '💥 Device ID not found',
          '⚠️ Signal name too long',
          '⚠️ Unused template reference'
        ]"
      />
    </BottomValidatorResizable>

    <!-- Navigation -->
    <SlideOver :open="isDrawerOpen" placement="right" title="Menu" :widthPx="360" @close="isDrawerOpen = false">
      <AppMenu />
    </SlideOver>

    <!-- Properties -->
    <SlideOver
      v-if="meta.rightAside"
      ref="propsPanel"
      :open="isPropsOpen"
      placement="bottom"
      :maxHeightVh="75"
      @close="selection.clear()"
    >
    <div class="slide-over-content">
      <PropertyPanel
        v-if="selection.selectedItem"
        :schema="resolveSchema(selection.selected!.type)"
        :item="selection.selectedItem!"
        @update="onUpdate"
      />

    </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch } from "vue"
import { useRoute } from "vue-router"

import { useSelectionOutside } from "@/composables/useSelectionOutside"

import EventLog from "./EventLog.vue"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import BottomValidatorResizable from "./BottomValidatorResizable.vue"
import BottomValidator from "./BottomValidator.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import SlideOver from "./SlideOver.vue"
import { useSelectionStore } from "@/stores/selectionStore"
import PropertyPanel from "./PropertyPanel.vue"
import { resolveSchema } from "@/property-schemas/propertySchemas"
import MobileHeader from "./MobileHeader.vue"
import { updateEntity } from "@/utils/updateEntity"


const propsPanel = ref<InstanceType<typeof SlideOver> | null>(null)

// also works for bottom sheet SlideOver
useSelectionOutside(() => propsPanel.value?.$el ?? null)

// Drawer state
const isDrawerOpen = ref(false)
const isPropsOpen = ref(false)

const selection = useSelectionStore()

watch(
  () => selection.selected,
  (val) => {
    isPropsOpen.value = val !== null
  }
)

// WebSocket connection status
const wsStore = useWebSocketStore()
const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

async function onUpdate(key: any, value: any) {
  if (!selection.selected || !selection.selectedItem) return
  const { type } = selection.selected
  await updateEntity(type as any, selection.selectedItem as any, key as any, value)
}

// Read route meta
const route = useRoute()
const meta = computed(() => ({
  rightAside: route.meta.rightAside ?? true,
  bottomValidator: route.meta.bottomValidator ?? true,
  globalEventLog: route.meta.globalEventLog ?? true,
  headerBulkActions: route.meta.headerBulkActions ?? false,
}))
</script>
