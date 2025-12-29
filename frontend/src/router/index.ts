// src/router/index.ts
import { createRouter, createWebHistory } from "vue-router"
import { sequencesRoutes } from './sequences.routes';

import { devicesRoutes } from "./devices.routes";
import { switchgearsRoutes } from "./switchgears.routes";

// Default layout meta (used by most screens)
const defaultMeta = {
  leftAside: true,
  layout: "auto" as "auto" | "app" | "mobile",
}

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: "/",
      name: "home",
      component: () => import("@/pages/home/HomePage.vue"),
    },
    ...devicesRoutes,
    ...switchgearsRoutes,
    ...sequencesRoutes,

    // Catch-all → redirect home
    {
      path: "/:pathMatch(.*)*",
      redirect: "/",
      meta: {
        ...defaultMeta,
      },
    },
  ],
})

export default router
