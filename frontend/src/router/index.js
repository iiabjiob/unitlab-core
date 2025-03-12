import { createRouter, createWebHistory } from 'vue-router';

import DashboardView from '../views/DashboardView.vue';
import EventLogView from '../views/EventLogView.vue';
import DiagnosticsView from '../views/DiagnosticsView.vue';

import settingsRoutes from "./settings"; // 🆕 Импортируем маршруты для настроек

const routes = [
  { path: '/', component: DashboardView },
  { path: '/events', component: EventLogView },
  { path: '/diagnostic', component: DiagnosticsView },
  ...settingsRoutes, // 🆕 Добавляем маршруты настроек
];

const router = createRouter({
  history: createWebHistory(),
  routes
});

export default router;
