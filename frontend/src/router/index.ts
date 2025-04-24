import { createRouter, createWebHistory, RouteRecordRaw } from 'vue-router'

import IOView from '@/views/IOView.vue'
import EventsView from '@/views/EventsView.vue'
import SystemView from '@/views/SystemView.vue'
import SettingsView from '@/views/SettingsView.vue'
import ProjectView from '@/views/settings/ProjectView.vue'
import DevicesView from '@/views/DevicesView.vue'

const routes: RouteRecordRaw[] = [
  { path: '/', component: IOView },
  { path: '/devices', component: DevicesView },
  { path: '/events', component: EventsView },
  { path: '/system', component: SystemView },
  {
    path: '/settings',
    component: SettingsView,
    children: [
      { path: '', redirect: 'settings/project' },
      { path: 'project', component: ProjectView },
    ]
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
