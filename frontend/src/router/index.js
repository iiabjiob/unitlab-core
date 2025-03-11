import { createRouter, createWebHistory } from 'vue-router';

import DashboardView from '../views/DashboardView.vue';
import EventLogView from '../views/EventLogView.vue';
import DiagnosticsView from '../views/DiagnosticsView.vue';
import SettingsView from '../views/SettingsView.vue';

const routes = [
  { path: '/', component: DashboardView },
  { path: '/event-log', component: EventLogView },
  { path: '/diagnostics', component: DiagnosticsView },
  { path: '/settings', component: SettingsView },
];

const router = createRouter({
  history: createWebHistory(),
  routes
});

export default router;
