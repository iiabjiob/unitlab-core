// src/router/index.ts
import { createRouter, createWebHistory } from "vue-router"
import { sequencesRoutes } from './sequences.routes';

// Views
import HomeView from "@/views/HomeView.vue"
import EventsView from "@/views/EventsView.vue"
import SwitchgearView from "@/views/SwitchgearView.vue"
import { devicesRoutes } from "./devices.routes";

// Default layout meta (used by most screens)
const defaultMeta = {
  // Layout chrome toggles
  leftAside: true,
  rightAside: true,
  bottomAside: true,

  // Layout selection (used by App.vue to swap desktop/mobile shells)
  // 'app' = desktop/regular shell, 'mobile' = force mobile shell;
  // leave as 'auto' to let App.vue choose by viewport breakpoints.
  layout: "auto" as "auto" | "app" | "mobile",
}

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: "/",
      name: "dashboard",
      component: HomeView,
      meta: {
        ...defaultMeta,
        rightAside: false,
        bottomAside: false,
      },
    },
    ...devicesRoutes,
    {
      path: "/switchgears",
      name: "switchgears",
      component: SwitchgearView,
      meta: {
        ...defaultMeta,
      },
    },
    ...sequencesRoutes,

    // Event List: clean "data view" layout
    {
      path: "/events",
      name: "events",
      component: EventsView,
      meta: {
        ...defaultMeta,
        leftAside: true,
        rightAside: false,
        bottomAside: false,
      },
    },

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
