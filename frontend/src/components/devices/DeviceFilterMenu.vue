<template>
  <Menu as="div" class="relative inline-block text-left">
    <div>
      <MenuButton
        as="button"
        class="btn-toolbar cursor-pointer"
        :class="{ 'btn-toolbar-active': hasFilters }"
      >
        <component :is="hasFilters ? FunnelPlusIcon : FunnelIcon" size="14" />
      </MenuButton>
    </div>

    <Transition enter="transition duration-100 ease-out" enter-from="transform scale-95 opacity-0"
      enter-to="transform scale-100 opacity-100" leave="transition duration-75 ease-in"
      leave-from="transform scale-100 opacity-100" leave-to="transform scale-95 opacity-0">
      <MenuItems
        class="absolute right-0 mt-3 w-52 p-3 origin-top-right bg-white dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded shadow-lg focus:outline-none text-sm space-y-3">
        <div>
          <p class="block text-xs text-neutral-500 mb-1">Device Status</p>
          <!-- Online only -->
          <label class="flex items-center gap-1">
            <input type="checkbox" v-model="filterStore.onlyOnline" />
            <span>Online only</span>
          </label>
        </div>

        <!-- Types -->
        <div>
          <p class="block text-xs text-neutral-500 mb-1">Types</p>
          <div class="flex flex-col gap-1">
            <label class="flex items-center gap-1">
              <input type="checkbox" value="di" v-model="filterStore.types" />
              <span>DI</span>
            </label>
            <label class="flex items-center gap-1">
              <input type="checkbox" value="do" v-model="filterStore.types" />
              <span>DO</span>
            </label>
            <label class="flex items-center gap-1">
              <input type="checkbox" value="ao" v-model="filterStore.types" />
              <span>AO</span>
            </label>
          </div>
        </div>

        <!-- Reset all -->
        <div class="pt-2 border-t border-neutral-200 dark:border-neutral-700">
          <UiButton type="secondary" size="xs" @click="resetFilters">
            Reset filters
          </UiButton>
        </div>

      </MenuItems>
    </Transition>
  </Menu>
</template>

<script setup lang="ts">
import { Menu, MenuButton, MenuItems } from "@headlessui/vue"
import { useDeviceFilterStore } from "@/stores/deviceFilterStore"
import FunnelIcon from "../icons/FunnelIcon.vue";
import { computed } from "vue";
import FunnelPlusIcon from "../icons/FunnelPlusIcon..vue";
import UiButton from "../ui/UiButton.vue";
const filterStore = useDeviceFilterStore()

const hasFilters = computed(() =>
  filterStore.onlyOnline || filterStore.types.length > 0
)

function resetFilters() {
  filterStore.onlyOnline = false
  filterStore.types = []
}

</script>
