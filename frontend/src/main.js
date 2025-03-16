import './assets/base.css'
import './assets/tailwind.css'

import { createApp } from 'vue'
import App from './App.vue'
import { loadConfig } from "./config";

import router from './router';

async function init() {
  await loadConfig(); // Загружаем конфиг перед стартом Vue
  createApp(App).use(router).mount('#app')
}

init();
