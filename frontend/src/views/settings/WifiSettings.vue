<script setup>
import { ref, onMounted } from "vue";
import axios from "axios";
import ButtonComponent from "@/components/ButtonComponent.vue";

const networks = ref([]);
const selectedNetwork = ref(null);
const password = ref("");
const isLoading = ref(false);
const message = ref("");

// Получение списка Wi-Fi сетей
const fetchNetworks = async () => {
  try {
    isLoading.value = true;
    const response = await axios.get("/api/wifi/scan", { baseURL: "/" });
    if (response.data.error) {
      message.value = response.data.error; // Выводим ошибку
      networks.value = []; // Очищаем список
    } else {
      networks.value = response.data; // Записываем список сетей
      message.value = ""; // Очищаем возможные прошлые ошибки
    }
  } catch (error) {
    console.error("Ошибка загрузки Wi-Fi сетей:", error);
  } finally {
    isLoading.value = false;
  }
};

// Подключение к Wi-Fi
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

    message.value = `Подключено к ${selectedNetwork.value}`;
  } catch (error) {
    message.value = error.response?.data?.detail || "Ошибка подключения";
  } finally {
    isLoading.value = false;
  }
};

// Автоматический запрос Wi-Fi сетей при загрузке
onMounted(fetchNetworks);
</script>

<template>
  <div>
    <!-- Кнопка для сканирования Wi-Fi -->
    <ButtonComponent @click="fetchNetworks">{{ isLoading ? "Scanning..." : "Scan Wi-Fi" }}</ButtonComponent>

    <!-- Вывод списка сетей -->
    <div v-if="networks.length && !message" class="w-full max-w-md">
      <p class="mb-2 text-gray-600">Chose network:</p>
      <ul class="space-y-2">
        <li v-for="network in networks" :key="network.ssid"
            @click="selectedNetwork = network.ssid"
            class="p-3 bg-white rounded-lg shadow cursor-pointer hover:bg-gray-200 transition">
          {{ network.ssid }} ({{ network.signal }}%)
        </li>
      </ul>
    </div>

    <!-- Поле ввода пароля -->
    <div v-if="selectedNetwork" class="mt-4 w-full max-w-md">
      <p class="mb-2 font-medium">Network: {{ selectedNetwork }}</p>

      <!-- TODO: use vue component for input -->
      <input v-model="password" type="password" placeholder="Enter password"
             class="w-full p-2 border rounded-lg focus:outline-none focus:ring focus:ring-blue-300">

      <ButtonComponent @click="connectToWifi">{{ isLoading ? "Connecting..." : "Connect" }}</ButtonComponent>

    </div>

    <!-- Сообщение об ошибке или статусе -->
    <!-- TODO: use vue component for input -->
    <p v-if="message" >{{ message }}</p>
  </div>
</template>
