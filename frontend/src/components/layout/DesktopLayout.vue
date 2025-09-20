<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="flex flex-1 overflow-hidden">
      <!-- Left aside -->
      <ResizablePanel
        v-if="meta.leftAside"
        class="bg-white dark:bg-neutral-800"
        placement="left"
        storageKey="left-aside-width"
        :defaultSize="240"
        :minSize="200"
        :maxSize="400"
      >
        <AppAside class="border-r border-neutral-200 dark:border-neutral-700"/>
      </ResizablePanel>

      <!-- Center workspace (main + bottom log) -->
      <div class="flex-1 flex flex-col overflow-hidden">

        <!-- UiToolbar -->
        <div v-if="meta.toolbar && meta.toolbarComponent" class="ignore-selection p-3">
          <component :is="meta.toolbarComponent" class="bg-white dark:bg-neutral-800" />
        </div>

        <!-- Main content -->
        <div class="flex-1 overflow-auto">
          <RouterView />
        </div>

        <!-- Bottom event log -->
        <ResizablePanel
          v-if="meta.bottomAside"
          class="ignore-selection bg-white dark:bg-neutral-800"
          placement="bottom"
          storageKey="bottom-aside-height"
          :defaultSize="240"
          :minSize="160"
          :maxSize="400"
        >
          <EventLog class="border-t border-neutral-200 dark:border-neutral-700" />
        </ResizablePanel>
      </div>

      <ResizablePanel
        v-if="meta.rightAside && showRightAside"
        class="ignore-selection bg-neutral-100 dark:bg-neutral-800"
        placement="right"
        storageKey="right-panel-width"
        :defaultSize="280"
        :minSize="200"
        :maxSize="500"
      >
        <div class="flex flex-col h-full border-l border-neutral-200 dark:border-neutral-700">
          <!-- Header -->
          <div class="flex items-center justify-between px-3 py-2 ">
            <h4 class="font-bold text-sm">Properties</h4>
            <button
              class="text-neutral-400 hover:text-neutral-700 dark:hover:text-neutral-200"
              @click="collapseRightAside"
            >
              ✕
            </button>
          </div>

          <!-- Body -->
          <div class="flex-1 overflow-auto p-3" v-if="selection.selectedItem">
            <PropertiesPanel
              :schema="resolveSchema(selection.selected!.type)"
              :schema-name="selection.selected!.type"
              :item="selection.selectedItem!"
              @update="onUpdate"
            />
          </div>

          <div
            v-else
            class="flex-1 flex items-center justify-center text-xs text-neutral-500"
          >
            No item selected
          </div>

          <!-- Toggle bar -->
          <div
            class="flex items-center justify-between px-3 py-1 text-xs border-t border-neutral-300 dark:border-neutral-700 bg-neutral-200 dark:bg-neutral-700 cursor-pointer"
            @click="validation.toggleValidator"
          >
            <div class="flex items-center gap-2">
              <h4 class="font-bold text-sm">Validator</h4>
              <span class="text-red-700 dark:text-red-300">🛑 {{ errorsCount }}</span>
              <span class="text-yellow-700 dark:text-yellow-300">⚠️ {{ warningsCount }}</span>
            </div>
            <span class="text-neutral-600 dark:text-neutral-300">
              {{ validation.showValidator ? "Hide" : "Show" }}
            </span>
          </div>

          <!-- Panel -->
          <ResizablePanel
            v-if="validation.showValidator"
            class="ignore-selection bg-neutral-100 dark:bg-neutral-800 border-t border-neutral-300 dark:border-neutral-700"
            placement="bottom"
            storageKey="validator-height"
            :defaultSize="validation.panelHeight"
            :minSize="100"
            :maxSize="300"
            @resize-end="validation.setPanelHeight"
          >
            <ValidatorPanel :errors="allErrors" @focus-field="focusField" />
          </ResizablePanel>
        </div>
      </ResizablePanel>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import AppAside from "./DesktopAside.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import EventLog from "../EventLog.vue"
import PropertiesPanel from "../properties/PropertiesPanel.vue"
import { useSelectionStore } from "@/stores/selectionStore"
import { updateEntity } from "@/property-schemas/updateEntity"
import { resolveSchema } from "@/property-schemas/propertySchemas"
import { useSelectionOutside } from "@/composables/useSelectionOutside"
import { useValidationStore } from "@/stores/validationStore"
import ValidatorPanel from "../ValidatorPanel.vue"
import type { ValidationError } from "@/property-schemas/validation"

useSelectionOutside()

import { useFocusField } from "@/composables/useFocusField"
const { focusField } = useFocusField()

const selection = useSelectionStore()

async function onUpdate(key: string, value: any) {
  if (!selection.selected || !selection.selectedItem) return
  const { type } = selection.selected
  await updateEntity(type as any, selection.selectedItem as any, key, value)
}

const showRightAside = ref(true)

function collapseRightAside() {
  showRightAside.value = false
  selection.clear()
}

// Автооткрытие при выборе нового элемента
watch(
  () => selection.selectedItem,
  (item) => {
    if (item) {
      showRightAside.value = true
    }
  }
)

const showValidator = ref(false)

const validation = useValidationStore()
const allErrors = computed(() => validation.errors)

const errorsCount = computed(() =>
  allErrors.value.filter(e => e.level !== "warning").length
)
const warningsCount = computed(() =>
  allErrors.value.filter(e => e.level === "warning").length
)

const route = useRoute()
const meta = computed(() => ({
  toolbar: route.meta.toolbar ?? true,
  leftAside: route.meta.leftAside ?? true,
  rightAside: route.meta.rightAside ?? true,
  bottomAside: route.meta.bottomAside ?? true,
  toolbarComponent: route.meta.toolbarComponent ?? null,
}))
</script>
