// src/router/index.ts
import { createRouter, createWebHistory } from "vue-router"
import { sequencesRoutes } from "./sequences.routes"
import { devicesRoutes } from "./devices.routes"
import { switchgearsRoutes } from "./switchgears.routes"
import { signalsRoutes } from "./signals.routes"
import { settingsRoutes } from "./settings.routes"
import { debug61850Routes } from "./debug61850.routes"
import { bootRuntime } from "@/boot/runtime"

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
      meta: {
        leftAside: false,
        layout: "welcome" as const,
      },
    },
    ...devicesRoutes,
    ...switchgearsRoutes,
    ...signalsRoutes,
    ...debug61850Routes,
    ...settingsRoutes,
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

// ✅ Runtime bootstrap (global, idempotent)
router.beforeEach(async () => {
  await bootRuntime()
})

export default router
