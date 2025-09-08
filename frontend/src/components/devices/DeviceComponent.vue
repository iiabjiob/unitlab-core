<template>
  <div
    class="device-card flex flex-col h-full p-3 rounded-md bg-white dark:bg-neutral-800 cursor-pointer shadow-sm border border-neutral-200 dark:border-neutral-700"
    :class="[
      selected
        ? 'border-neutral-500 dark:border-neutral-500'
        : '',
    ]"
    @click="$emit('select', device)"
  >
    <!-- Верхняя строка -->
    <div class="flex items-center justify-between">
      <!-- ID/Name + статус + location -->
      <div class="flex items-center gap-2 flex-wrap">

        <span class="font-mono font-semibold">
          {{ device.name?.trim() || device.unit_id }}
        </span>

        <!-- Location (optional) -->
        <BadgeComponent v-if="device.location">Location: {{ device.location }}</BadgeComponent>

        <OnlineStatusComponent :status="device.status"/>

      </div>

      <!-- меню действий -->
      <Menu as="div" class="relative inline-block text-left" @click.stop>
        <div>
          <MenuButton
            @click.stop
            class="flex items-center justify-center rounded-full w-6 h-6 text-neutral-500 hover:bg-neutral-100 dark:hover:bg-neutral-700 cursor-pointer text-xl font-bold"
          >
            ⋮
          </MenuButton>
        </div>

        <Transition
          enter="transition ease-out duration-100"
          enter-from="transform opacity-0 scale-95"
          enter-to="transform opacity-100 scale-100"
          leave="transition ease-in duration-75"
          leave-from="transform opacity-100 scale-100"
          leave-to="transform opacity-0 scale-95"
        >
          <MenuItems
            @click.stop
            class="absolute right-0 mt-2 w-40 origin-top-right rounded-md bg-white dark:bg-neutral-700 shadow-lg ring-1 ring-neutral-200 dark:ring-neutral-600 ring-opacity-5 focus:outline-none z-10"
          >
            <div class="py-1">
              <MenuItem v-slot="{ active }">
                <button
                  @click="toggleActive"
                  @click.stop="toggleActive"
                  :class="[
                    active ? 'bg-neutral-100 dark:bg-neutral-600' : '',
                    'block w-full px-4 py-2 text-sm text-left text-neutral-700 dark:text-neutral-200'
                  ]"
                >
                  {{ device.is_active ? 'Deactivate' : 'Activate' }}
                </button>
              </MenuItem>
              <MenuItem v-slot="{ active }">
                <button
                  @click="deleteDev"
                  @click.stop="deleteDev"
                  :class="[
                    active ? 'bg-neutral-100 dark:bg-neutral-600' : '',
                    'block w-full px-4 py-2 text-sm text-left text-red-600 dark:text-red-400'
                  ]"
                >
                  Delete
                </button>
              </MenuItem>
            </div>
          </MenuItems>
        </Transition>
      </Menu>
    </div>

    <!-- Каналы -->
    <div class="mt-3">
      <ChannelsComponent
        v-if="device.status === 'online'"
        :unit-id="device.unit_id"
        :device-type="device.type"
        :channels="device.channels"
      />

      <ChannelsPlaceholder
        v-else
        :unit-id="device.unit_id"
        :device-type="device.type"
        :channels="device.channels"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { Menu, MenuButton, MenuItems, MenuItem } from "@headlessui/vue"
import { useDeviceStore } from "@/stores/deviceStore"
import type { Device } from "@/types/device"
import ChannelsComponent from "./ChannelsComponent.vue"
import BadgeComponent from "../ui/BadgeComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import ChannelsPlaceholder from "./ChannelsPlaceholder.vue"

const props = defineProps<{
  device: Device
  selected?: boolean
}>()
const deviceStore = useDeviceStore()

const emit = defineEmits<{ (e: 'select', device: Device): void }>()

async function toggleActive() {
  await deviceStore.toggleDeviceActive(props.device.unit_id)
}

async function deleteDev() {
  if (confirm(`Delete device ${props.device.unit_id}?`)) {
    await deviceStore.deleteDevice(props.device.unit_id)
  }
}
</script>
