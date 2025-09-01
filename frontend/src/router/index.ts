import { createRouter, createWebHistory } from 'vue-router'
import HomeView from '@/views/HomeView.vue'
import DevicesView from '@/views/DevicesView.vue'
import EventsView from '@/views/EventsView.vue'
import SwitchgearView from '@/views/SwitchgearView.vue'
import SignalListView from '@/views/SignalListView.vue'
import SequencesView from '@/views/SequencesView.vue'

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
      path: '/switchgear',
      name: 'switchgear',
      component: SwitchgearView,
    },
    {
      path: '/signal-list',
      name: 'signal-list',
      component: SignalListView,
    },
    {
      path: '/sequences',
      name: 'sequences',
      component: SequencesView,
    },
    {
      path: '/events',
      name: 'events',
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
