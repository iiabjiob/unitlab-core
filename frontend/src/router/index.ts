// src/router/index.ts
import { createRouter, createWebHistory } from "vue-router"
import { h } from "vue"

// Views
import HomeView from "@/views/HomeView.vue"
import DevicesView from "@/views/DevicesView.vue"
import EventsView from "@/views/EventsView.vue"
import SwitchgearView from "@/views/SwitchgearView.vue"
import SignalListView from "@/views/SignalListView.vue"
import SequencesView from "@/views/SequencesView.vue"
import SettingsView from "@/views/SettingsView.vue"

// Default layout meta (used by most screens)
const defaultMeta = {
  // Layout chrome toggles
  leftAside: true,
  rightAside: true,
  bottomValidator: true,
  globalEventLog: true,
  headerBulkActions: false,

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
        // example: dashboard usually needs full chrome
      },
    },
    {
      path: "/devices",
      name: "devices",
      component: DevicesView,
      meta: {
        ...defaultMeta,
        headerBulkActions: true,
      },

    },
    {
      path: "/switchgear",
      name: "switchgear",
      component: SwitchgearView,
      meta: {
        ...defaultMeta,
      },
    },
    {
      path: "/signal-list",
      name: "signal-list",
      component: SignalListView,
      meta: {
        ...defaultMeta,
        // optionally force mobile shell on phones only:
        // layout: "auto",
      },
    },
    {
      path: "/sequences",
      name: "sequences",
      component: SequencesView,
      meta: {
        ...defaultMeta,
      },
    },
    {
      path: "/settings",
      name: "settings",
      component: SettingsView,
      meta: {
        ...defaultMeta,
      },
    },

    // Event List: clean "data view" layout
    {
      path: "/events",
      name: "events",
      component: EventsView,
      meta: {
        ...defaultMeta,
        leftAside: true,
        rightAside: false,
        bottomValidator: false,
        globalEventLog: false,
        headerBulkActions: false,
        // keep 'auto' so phones still get the mobile shell
        layout: "auto",
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
