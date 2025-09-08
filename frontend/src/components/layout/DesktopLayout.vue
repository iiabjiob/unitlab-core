<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="flex flex-1 overflow-hidden">
      <!-- Left aside -->
      <LeftAsideResizable v-if="meta.leftAside">
        <AppAside />
      </LeftAsideResizable>

      <!-- Workspace -->
      <div class="flex-1 flex flex-col overflow-hidden relative">
        <!-- Header bulk actions -->
        <div v-if="meta.headerBulkActions" class="shrink-0">
          <slot name="header-bulk-actions" />
        </div>
        <div class="flex flex-1 overflow-hidden">
          <main class="flex-1 overflow-auto">
            <RouterView />
          </main>

          <!-- Right aside -->
          <RightAsideResizable
            v-if="meta.rightAside"
            ref="rightAside"
            class="absolute top-0 right-0 h-full shadow-lg z-20"
          />

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
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"
import { useRoute } from "vue-router"

import AppAside from "./AppAside.vue"
import BottomValidator from "./BottomValidator.vue"
import LeftAsideResizable from "./LeftAsideResizable.vue"
import RightAsideResizable from "./RightAsideResizable.vue"
import BottomValidatorResizable from "./BottomValidatorResizable.vue"
import { onMounted, onBeforeUnmount } from "vue"
import { useSelectionStore } from "@/stores/selectionStore"

const selection = useSelectionStore()

function handleClickOutside(e: MouseEvent) {

  const target = e.target as HTMLElement

  if (target.closest(".device-card")) return

  if (rightAside.value && rightAside.value.$el.contains(target)) return

  selection.clear()
}

onMounted(() => document.addEventListener("click", handleClickOutside))
onBeforeUnmount(() => document.removeEventListener("click", handleClickOutside))

const rightAside = ref<InstanceType<typeof RightAsideResizable> | null>(null)

// Read meta flags from current route
const route = useRoute()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
  rightAside: route.meta.rightAside ?? true,
  bottomValidator: route.meta.bottomValidator ?? true,
  headerBulkActions: route.meta.headerBulkActions ?? false,
}))
</script>
