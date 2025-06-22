import './assets/base.css'
import './assets/tailwind.css'

import { createApp } from 'vue'
import { createPinia } from 'pinia'

import App from './App.vue'

import router from './router';

// import { AllCommunityModule, ModuleRegistry } from 'ag-grid-community';
// ModuleRegistry.registerModules([AllCommunityModule]);

const pinia = createPinia()
const app = createApp(App)

app.use(pinia)
app.use(router)

app.mount('#app')
