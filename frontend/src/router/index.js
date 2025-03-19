import { createRouter, createWebHistory } from 'vue-router';

import DashboardView from '@/views/DashboardView.vue';
import EventsView from '@/views/EventsView.vue';
import SystemView from '@/views/SystemView.vue';
import SettingsView from '@/views/settings/SettingsView.vue';
import WifiSettings from '@/views/settings/WifiSettings.vue';

const routes = [
  { path: '/', component: DashboardView },
  { path: '/events', component: EventsView },
  { path: '/system', component: SystemView },
  { path: '/settings', component: SettingsView },
  { path: '/settings/wifi', component: WifiSettings },
];

const router = createRouter({
  history: createWebHistory(),
  routes
});

export default router;
