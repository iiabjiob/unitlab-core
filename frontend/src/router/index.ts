import { createRouter, createWebHistory, RouteRecordRaw } from 'vue-router'

import DevicesView from '@/views/DevicesView.vue'

const routes: RouteRecordRaw[] = [
  { path: '/devices', component: DevicesView },
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
