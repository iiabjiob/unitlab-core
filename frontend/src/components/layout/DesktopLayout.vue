<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="flex flex-1 overflow-hidden">
      <!-- Left aside (resizable) -->
      <LeftAsideResizable>
        <AppAside />
      </LeftAsideResizable>

      <!-- Workspace (main + right + bottom) -->
      <div class="flex-1 flex flex-col overflow-hidden relative">
        <!-- Content row (main + right aside) -->
        <div class="flex flex-1 overflow-hidden">
          <main class="flex-1 overflow-auto">
            <RouterView />
          </main>

          <!-- Right aside -->
          <RightAsideResizable
            ref="rightAside"
            class="border-l border-neutral-200 dark:border-neutral-800"
          />

          <!-- Expand handle -->
          <button
            v-if="rightAside?.collapsed"
            class="absolute top-1/2 right-0 -translate-y-1/2 w-5 h-16 flex items-center justify-center bg-neutral-200 dark:bg-neutral-700 rounded-l cursor-pointer"
            @click="rightAside.collapsed = false"
          >
            ‹
          </button>
        </div>

        <!-- Bottom validator (only inside workspace, like VS Code Problems) -->
        <BottomValidatorResizable :errors="3" :warnings="2">
          <BottomValidator
            :messages="[
              '❌ Device ID not found',
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
import { ref } from "vue"
import AppAside from "./AppAside.vue"
import BottomValidator from "./BottomValidator.vue"
import LeftAsideResizable from "./LeftAsideResizable.vue"
import RightAsideResizable from "./RightAsideResizable.vue"
import BottomValidatorResizable from "./BottomValidatorResizable.vue"

const rightAside = ref<InstanceType<typeof RightAsideResizable> | null>(null)
</script>
