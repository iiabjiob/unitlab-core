import './assets/main.css'

import { createApp } from 'vue'
import { createPinia } from 'pinia'
import piniaPluginPersistedstate from "pinia-plugin-persistedstate"

import App from './App.vue'
import router from './router'
import { logger } from './utils/logger'
import { bootPreload } from './boot/preload'

const app = createApp(App)

const pinia = createPinia()
pinia.use(piniaPluginPersistedstate)

app.use(pinia)
app.use(router)

logger.info('🚀 Starting frontend application')

bootPreload()

app.mount('#app')

logger.info('✅ Frontend application is up')
