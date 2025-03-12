import { createRouter, createWebHistory } from 'vue-router';

import DashboardView from '../views/DashboardView.vue';
import EventsView from '../views/EventsView.vue';
import SystemView from '@/views/SystemView.vue';

import settingsRoutes from "./settings"; // 🆕 Импортируем маршруты для настроек

const routes = [
  { path: '/', component: DashboardView },
  { path: '/events', component: EventsView },
  { path: '/system', component: SystemView },
  ...settingsRoutes, // 🆕 Добавляем маршруты настроек
];

const router = createRouter({
  history: createWebHistory(),
  routes
});

export default router;
