import { createRouter, createWebHistory } from 'vue-router'
import HomeView from '@/views/HomeView.vue'
import DevicesView from '@/views/DevicesView.vue'
import EventsView from '@/views/EventsView.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      name: 'dashboard',
      component: HomeView,
    },
    {
      path: '/devices',
      name: 'devices',
      component: DevicesView,
    },
    {
      path: '/events',
      name: 'events',
      // TODO: to right view
      component: EventsView,
    },
    // перехватываем все неизвестные пути
    {
      path: '/:pathMatch(.*)*',
      redirect: '/',
    },
  ],
})

export default router
