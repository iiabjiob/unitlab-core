import './assets/main.css'
import 'floating-vue/dist/style.css'

import { createApp } from 'vue'
import { createPinia } from 'pinia'

import { initWebSocket } from './wsInit'

import App from './App.vue'
import router from './router'
import { logger } from './utils/logger'

const app = createApp(App)

app.use(createPinia())
app.use(router)

logger.info('🚀 Starting frontend application')

app.mount('#app')

initWebSocket()

logger.info('✅ Frontend application is up')
