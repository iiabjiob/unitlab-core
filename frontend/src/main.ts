import './assets/main.css'
import 'floating-vue/dist/style.css'

import { createApp } from 'vue'
import { createPinia } from 'pinia'

import App from './App.vue'
import router from './router'
import { logger } from './utils/logger'

const app = createApp(App)

app.use(createPinia())
app.use(router)

logger.info('🚀 Starting frontend application')

app.mount('#app')

logger.info('✅ Frontend application is up')
