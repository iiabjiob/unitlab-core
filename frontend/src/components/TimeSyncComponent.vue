<script setup>
import { ref, computed, onMounted, onUnmounted, watchEffect } from "vue";
import { useMqtt } from "@/composables/useMqtt";

const { data: timeSync } = useMqtt("time_sync"); // Subscribe to MQTT topic
const currentTime = ref(new Date()); // Local time
let syncAvailable = ref(false); // Is external sync available?
let clockInterval = null; // Interval for incrementing time

// Formatting functions
const formattedDate = computed(() =>
  currentTime.value.toLocaleDateString("en-GB", { year: "numeric", month: "2-digit", day: "2-digit" })
);
const formattedTime = computed(() =>
  currentTime.value.toLocaleTimeString("en-GB", { hour: "2-digit", minute: "2-digit", second: "2-digit" })
);

// Function to increment time if no sync source
const incrementTime = () => {
  if (!syncAvailable.value) {
    currentTime.value = new Date(currentTime.value.getTime() + 1000); // +1 second
  }
};

// Watch for MQTT messages
watchEffect(() => {
  if (timeSync.value?.synchronized && timeSync.value.current_time) {
    currentTime.value = new Date(timeSync.value.current_time);
    syncAvailable.value = true; // External sync is available
  } else {
    syncAvailable.value = false; // No sync source
  }
});

// Start local clock update every second
onMounted(() => {
  clockInterval = setInterval(incrementTime, 1000);
});

// Cleanup
onUnmounted(() => {
  clearInterval(clockInterval);
});
</script>

<template>
  <div class="inline-flex gap-1 items-center text-sm text-gray-500">
    <span v-if="timeSync?.synchronized">{{ timeSync.source }}</span>
    <span v-else>*</span>
    <span>{{ formattedDate }}</span>
    <span class="tabular-nums">{{ formattedTime }}</span>
  </div>
</template>
