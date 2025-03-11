<template>
  <div>

    <div class="mb-4">
      <p class="text-gray-700"><strong>Current Mode:</strong> {{ wifiMode }}</p>
      <div class="mt-2">
        <button
          v-if="wifiMode !== 'client'"
          @click="setClientMode"
          class="px-4 py-2 bg-blue-500 text-white hover:bg-blue-600"
        >
          Client Mode
        </button>
        <button
          v-if="wifiMode !== 'ap'"
          @click="setApMode"
          class="px-4 py-2 bg-green-500 text-white hover:bg-green-600"
        >
          Access Point
        </button>
      </div>
    </div>

    <h4 class="text-lg font-semibold mt-6 mb-2">Available Networks</h4>
    <button @click="fetchWifiNetworks" class="mb-4 px-4 py-2 bg-gray-500 text-white rounded-lg hover:bg-gray-600">
      Refresh List
    </button>

    <ul class="border rounded-lg">
      <li
        v-for="network in wifiNetworks"
        :key="network.ssid"
        class="p-2 flex justify-between items-center border-b last:border-none"
      >
        <span>{{ network.ssid }} ({{ network.signal }}%)</span>
        <button
          @click="connectToWifi(network.ssid)"
          class="px-3 py-1 bg-blue-500 text-white rounded hover:bg-blue-600"
        >
          Connect
        </button>
      </li>
    </ul>

    <div v-if="selectedSSID" class="mt-6 p-4 bg-gray-100 rounded-lg">
      <h4 class="text-lg font-semibold mb-2">Connecting to {{ selectedSSID }}</h4>
      <input
        v-model="password"
        type="password"
        placeholder="Enter password"
        class="w-full p-2 border rounded-lg mb-2"
      />
      <button @click="confirmConnection" class="px-4 py-2 bg-blue-500 text-white rounded-lg hover:bg-blue-600">
        Connect
      </button>
    </div>

    <p v-if="statusMessage" class="mt-4 text-gray-700">{{ statusMessage }}</p>
  </div>
</template>

<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";

const wifiNetworks = ref([]);
const wifiMode = ref("");
const selectedSSID = ref("");
const password = ref("");
const statusMessage = ref("");

const fetchWifiMode = async () => {
  try {
    const response = await axios.get("/wifi/mode");
    wifiMode.value = response.data.mode;
  } catch (error) {
    console.error("Error retrieving Wi-Fi mode:", error);
  }
};

const fetchWifiNetworks = async () => {
  try {
    const response = await axios.get("/wifi/list");
    wifiNetworks.value = response.data;
  } catch (error) {
    console.error("Error loading available networks:", error);
  }
};

const setClientMode = async () => {
  try {
    await axios.post("/wifi/set_client");
    statusMessage.value = "Switched to Client Mode";
    fetchWifiMode();
  } catch {
    statusMessage.value = "Error switching to Client Mode";
  }
};

const setApMode = async () => {
  try {
    await axios.post("/wifi/set_ap");
    statusMessage.value = "Switched to Access Point Mode";
    fetchWifiMode();
  } catch {
    statusMessage.value = "Error switching to Access Point Mode";
  }
};

const connectToWifi = (ssid) => {
  selectedSSID.value = ssid;
};

const confirmConnection = async () => {
  try {
    await axios.post("/wifi/connect", {
      ssid: selectedSSID.value,
      password: password.value || null,
    });
    statusMessage.value = `Connected to ${selectedSSID.value}`;
  } catch {
    statusMessage.value = "Error connecting to Wi-Fi";
  }
};

onMounted(() => {
  fetchWifiMode();
  fetchWifiNetworks();
});
</script>
