<template>
  <div>
    <PageHeader title="Settings" />

    <div class="flex mb-4">
      <button
        class="px-4 py-2 font-semibold border-b-2"
        :class="activeTab === 'wifi' ? 'border-blue-500 text-blue-500' : 'border-transparent text-gray-500'"
        @click="activeTab = 'wifi'"
      >
        Wi-Fi
      </button>
      <button
        class="px-4 py-2 font-semibold border-b-2"
        :class="activeTab === 'other' ? 'border-blue-500 text-blue-500' : 'border-transparent text-gray-500'"
        @click="activeTab = 'other'"
      >
        Other
      </button>
    </div>

    <div v-if="activeTab === 'wifi'">
      <WifiSettings />
    </div>
    <div v-if="activeTab === 'other'">
      <p>Other...</p>
    </div>
  </div>
</template>

<script setup>
import { ref } from "vue";
import PageHeader from "@/components/PageHeader.vue";
import WifiSettings from "@/components/WifiSettings.vue";

const activeTab = ref("wifi");
</script>
