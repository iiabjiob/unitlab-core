<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";
import ButtonComponent from "@/components/ButtonComponent.vue";
import AlertComponent from "@/components/AlertComponent.vue";
import InputComponent from "@/components/InputComponent.vue";

const networks = ref([]);
const selectedNetwork = ref(null);
const password = ref("");
const isLoading = ref(false);
const message = ref("");

// Fetch available Wi-Fi networks
const fetchNetworks = async () => {
  try {
    isLoading.value = true;
    const response = await axios.get("/api/wifi/scan", { baseURL: "/" });
    if (response.data.error) {
      message.value = response.data.error; // Show error message
      networks.value = []; // Clear network list
    } else {
      networks.value = response.data; // Save network list
      message.value = ""; // Clear previous errors
    }
  } catch (error) {
    console.error("Failed to load Wi-Fi networks:", error);
    message.value = "An error occurred while fetching Wi-Fi networks.";
  } finally {
    isLoading.value = false;
  }
};

// Connect to Wi-Fi
const connectToWifi = async () => {
  if (!selectedNetwork.value) return;
  try {
    isLoading.value = true;
    await axios.post("/api/wifi/connect", {
      ssid: selectedNetwork.value,
      password: password.value,
    }, {
      baseURL: "/"
    });

    message.value = `Connected to ${selectedNetwork.value}`;
  } catch (error) {
    message.value = error.response?.data?.detail || "Failed to connect to Wi-Fi.";
  } finally {
    isLoading.value = false;
  }
};

// Automatically fetch networks on mount
onMounted(fetchNetworks);
</script>

<template>
  <div>
    <!-- Scan Wi-Fi button -->
    <ButtonComponent @click="fetchNetworks">
      {{ isLoading ? "Scanning..." : "Scan Wi-Fi" }}
    </ButtonComponent>


    <!-- Wi-Fi network list -->
    <div v-if="networks.length && !message" class="w-full max-w-md">
      <p class="mb-2 text-gray-600">Choose a network:</p>
      <ul class="space-y-2">
        <li v-for="network in networks" :key="network.ssid"
            @click="selectedNetwork = network.ssid"
            class="p-3 bg-white rounded-lg shadow cursor-pointer hover:bg-gray-200 transition">
          {{ network.ssid }} ({{ network.signal }}%)
        </li>
      </ul>
    </div>

    <!-- Password input field -->
    <div v-if="selectedNetwork" class="mt-4 w-full max-w-md">
      <p class="mb-2 font-medium">Network: {{ selectedNetwork }}</p>

      <InputComponent v-model="password" type="password" placeholder="Enter password"/>

      <ButtonComponent @click="connectToWifi">
        {{ isLoading ? "Connecting..." : "Connect" }}
      </ButtonComponent>
    </div>

    <!-- Error or status message -->
    <AlertComponent v-if="message" :message="message" type="error"/>
  </div>
</template>
