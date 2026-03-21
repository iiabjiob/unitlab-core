import './assets/main.css'

import { createApp } from 'vue'

import App from './App.vue'
import router from './router'
import { logger } from './utils/logger'
import { bootPreload } from './boot/preload'
import { ensureAppOverlayHost } from './utils/overlayHost'
import { pinia } from './stores/pinia'

const app = createApp(App)

ensureAppOverlayHost()

app.use(pinia)
app.use(router)

logger.info('🚀 Starting frontend application')

bootPreload()

app.mount('#app')

logger.info('✅ Frontend application is up')