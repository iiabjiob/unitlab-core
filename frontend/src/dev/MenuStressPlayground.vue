<script setup lang="ts">
import { ref, reactive } from "vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
  UiMenuSeparator,
  UiMenuLabel,
  UiSubMenu,
  UiSubMenuTrigger,
  UiSubMenuContent,
} from "@/components/ui/menu"

// SETTINGS
const itemCounts = [50, 200, 500, 1000]
const selectedCount = ref(200)

const dynamicItems = reactive(
  Array.from({ length: 20 }, (_, i) => ({ id: i + 1, label: `Dynamic ${i + 1}` }))
)

// FLAGS
const enableScrollableContainer = ref(false)
const enableTransform = ref(false)
const enableRTL = ref(false)
const enableNested = ref(true)

// handlers
function onSelect(label: string | number) {
  console.log("Selected:", label)
}

function onDanger() {
  console.warn("Danger clicked")
}

function addDynamic() {
  const id = dynamicItems.length + 1
  dynamicItems.push({ id, label: `Dynamic ${id}` })
}

function removeDynamic() {
  dynamicItems.pop()
}
</script>

<template>
  <div
    class="menu-stress-playground p-10 space-y-10"
    :dir="enableRTL ? 'rtl' : 'ltr'"
  >
    <h1 class="text-2xl font-bold">Menu Stress Playground</h1>

    <!-- SETTINGS PANEL -->
    <div class="p-4 border rounded bg-neutral-100 dark:bg-neutral-800 flex flex-wrap gap-6">
      <div>
        <label class="font-medium block mb-1">Items count</label>
        <select v-model="selectedCount" class="px-3 py-1 rounded bg-white dark:bg-neutral-700">
          <option v-for="n in itemCounts" :key="n" :value="n">{{ n }}</option>
        </select>
      </div>

      <div>
        <label class="font-medium block mb-1">Scrollable Container</label>
        <input type="checkbox" v-model="enableScrollableContainer" />
      </div>

      <div>
        <label class="font-medium block mb-1">Transform Container</label>
        <input type="checkbox" v-model="enableTransform" />
      </div>

      <div>
        <label class="font-medium block mb-1">RTL Mode</label>
        <input type="checkbox" v-model="enableRTL" />
      </div>

      <div>
        <label class="font-medium block mb-1">Nested Submenus</label>
        <input type="checkbox" v-model="enableNested" />
      </div>

      <div>
        <label class="font-medium block mb-1">Dynamic Items</label>
        <div class="flex gap-2">
          <button class="px-3 py-1 bg-neutral-300 dark:bg-neutral-600 rounded" @click="addDynamic">Add</button>
          <button class="px-3 py-1 bg-neutral-300 dark:bg-neutral-600 rounded" @click="removeDynamic">Remove</button>
        </div>
      </div>
    </div>

    <!-- TEST TARGET -->
    <div
      class="p-10 border rounded bg-neutral-50 dark:bg-neutral-900"
      :style="{
        height: enableScrollableContainer ? '300px' : 'auto',
        overflow: enableScrollableContainer ? 'auto' : 'visible',
        transform: enableTransform ? 'scale(0.9)' : 'none',
      }"
    >
      <UiMenu>
        <UiMenuTrigger trigger="both" asChild>
          <button class="px-4 py-2 bg-blue-300 dark:bg-blue-700 rounded">
            Open Stress-Test Menu
          </button>
        </UiMenuTrigger>

        <UiMenuContent class="menu-playground-panel">
          <UiMenuLabel>Root items ({{ selectedCount }})</UiMenuLabel>
          <UiMenuSeparator />

          <!-- RENDER MANY ITEMS -->
          <UiMenuItem
            v-for="n in selectedCount"
            :key="n"
            @select="onSelect(n)"
          >
            Item {{ n }}
          </UiMenuItem>

          <!-- DYNAMIC SECTION -->
          <UiMenuSeparator />
          <UiMenuLabel>Dynamic Items ({{ dynamicItems.length }})</UiMenuLabel>

          <UiMenuItem
            v-for="item in dynamicItems"
            :key="item.id"
            @select="onSelect(item.label)"
          >
            {{ item.label }}
          </UiMenuItem>

          <!-- NESTED MENU -->
          <UiMenuSeparator />

          <UiSubMenu v-if="enableNested">
            <UiSubMenuTrigger>Nested Level 1 →</UiSubMenuTrigger>
            <UiSubMenuContent class="menu-playground-panel">
              <UiMenuLabel>Level 1</UiMenuLabel>
              <UiMenuSeparator />

              <UiMenuItem @select="onSelect('L1-A')">Level 1 — A</UiMenuItem>

              <UiSubMenu>
                <UiSubMenuTrigger>Nested Level 2 →</UiSubMenuTrigger>
                <UiSubMenuContent class="menu-playground-panel">
                  <UiMenuLabel>Level 2</UiMenuLabel>
                  <UiMenuSeparator />

                  <UiMenuItem @select="onSelect('L2-A')">Level 2 — A</UiMenuItem>
                  <UiMenuItem @select="onSelect('L2-B')">Level 2 — B</UiMenuItem>

                  <UiSubMenu>
                    <UiSubMenuTrigger>Nested Level 3 →</UiSubMenuTrigger>
                    <UiSubMenuContent class="menu-playground-panel">
                      <UiMenuLabel>Level 3</UiMenuLabel>
                      <UiMenuSeparator />

                      <UiMenuItem @select="onSelect('L3-A')">Level 3 — A</UiMenuItem>
                      <UiMenuItem @select="onSelect('L3-B')">Level 3 — B</UiMenuItem>
                      <UiMenuItem @select="onSelect('L3-C')">Level 3 — C</UiMenuItem>

                      <UiMenuSeparator />
                      <UiMenuItem danger @select="onDanger">Dangerous</UiMenuItem>
                    </UiSubMenuContent>
                  </UiSubMenu>
                </UiSubMenuContent>
              </UiSubMenu>
            </UiSubMenuContent>
          </UiSubMenu>

          <UiMenuSeparator />
          <UiMenuItem danger @select="onDanger">Delete Something</UiMenuItem>
        </UiMenuContent>
      </UiMenu>
    </div>
  </div>
</template>

<style scoped>
:global(.menu-playground-panel[data-state="closed"]) {
  opacity: 0;
  transform: translateY(-4px);
  pointer-events: none;
}

:global(.menu-playground-panel[data-state="open"]) {
  opacity: 1;
  transform: translateY(0);
  transition: opacity 120ms ease, transform 120ms ease;
}

:global(.menu-playground-panel[data-motion="from-bottom"][data-state="closed"]) {
  opacity: 0;
  transform: translateY(4px) scale(0.96);
}

:global(.menu-playground-panel[data-motion="from-bottom"][data-state="open"]) {
  opacity: 1;
  transform: translateY(0) scale(1);
  transition: opacity 140ms ease, transform 140ms cubic-bezier(.2, .8, .4, 1);
}

:global(.menu-playground-panel[data-motion="from-left"][data-state="closed"]) {
  opacity: 0;
  transform: translateX(-6px);
}

:global(.menu-playground-panel[data-motion="from-left"][data-state="open"]) {
  opacity: 1;
  transform: translateX(0);
  transition: opacity 120ms ease, transform 120ms ease;
}
</style>