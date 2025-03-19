import './assets/base.css'
import './assets/tailwind.css'

import { createApp } from 'vue'
import App from './App.vue'

import router from './router';

async function init() {
  createApp(App).use(router).mount('#app')
}

init();
