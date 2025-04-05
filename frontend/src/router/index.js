import { createRouter, createWebHistory } from 'vue-router';

import IOView from '@/views/IOView.vue';
import EventsView from '@/views/EventsView.vue';
import SystemView from '@/views/SystemView.vue';
import SettingsView from '@/views/SettingsView.vue';

const routes = [
  { path: '/', component: IOView },
  { path: '/events', component: EventsView },
  { path: '/system', component: SystemView },
  { path: '/settings', component: SettingsView },
];

const router = createRouter({
  history: createWebHistory(),
  routes
});

export default router;
