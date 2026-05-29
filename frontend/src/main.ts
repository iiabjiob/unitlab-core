import './assets/main.css'

import { createApp } from 'vue'

import App from './App.vue'
import router from './router'
import { logger } from './utils/logger'
import { bootPreload } from './boot/preload'
import { pinia } from './stores/pinia'

const app = createApp(App)

app.use(pinia)
app.use(router)

logger.info('🚀 Starting frontend application')

bootPreload()

app.mount('#app')

logger.info('✅ Frontend application is up')
