<template>
  <div class="p-5">
    <DeviceList :devices="devices" />
  </div>
</template>

<script setup lang="ts">

import { computed, ref } from 'vue'
import { useDeviceStore } from '@/stores/useDeviceStore'
// import { useSignalStore } from '@/stores/useSignalStore' // если будешь получать каналы из стора

import DeviceList from '@/components/DeviceList.vue'
import { Device } from '@/types/device'

// TODO: Временно для мокирования
// const deviceStore = useDeviceStore()
// const devices = computed(() => deviceStore.devices)
// const { devices } = useDeviceStore()

const devices = ref<Device[]>([
    {
      unit_id: 'DO-unit-ABCD',
      name: 'DO Unit #1',
      type: 'DO',
      is_active: true,
      is_online: true,
      channels: 4,
    },
    {
      unit_id: 'AO-unit-XYZ',
      name: 'AO Module #1',
      type: 'AO',
      is_active: true,
      is_online: true,
      channels: 2,
    },
    {
      unit_id: 'DI-unit-QWER',
      name: 'DI Board #1',
      type: 'DI',
      is_active: false,
      is_online: false,
      channels: 8,
    }
  ])

</script>
