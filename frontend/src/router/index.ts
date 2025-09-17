// src/router/index.ts
import { createRouter, createWebHistory } from "vue-router"

// Views
import HomeView from "@/views/HomeView.vue"
import DevicesView from "@/views/DevicesView.vue"
import EventsView from "@/views/EventsView.vue"
import SwitchgearView from "@/views/SwitchgearView.vue"
import SignalListView from "@/views/SignalListView.vue"
import SequencesView from "@/views/SequencesView.vue"
import SettingsView from "@/views/SettingsView.vue"
import DashboardToolbar from "@/components/toolbars/DashboardToolbar.vue"
import DevicesToolbar from "@/components/toolbars/DevicesToolbar.vue"
import SwitchgearsToolbar from "@/components/toolbars/SwitchgearsToolbar.vue"
import SignalListToolbar from "@/components/toolbars/SignalListToolbar.vue"
import SequencesToolbar from "@/components/toolbars/SequencesToolbar.vue"
import SettingsToolbar from "@/components/toolbars/SettingsToolbar.vue"
import EventsToolbar from "@/components/toolbars/EventsToolbar.vue"

// Default layout meta (used by most screens)
const defaultMeta = {
  // Layout chrome toggles
  toolbar: true,
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
        toolbarComponent: DashboardToolbar,
        // example: dashboard usually needs full chrome
      },
    },
    {
      path: "/devices",
      name: "devices",
      component: DevicesView,
      meta: {
        ...defaultMeta,
        toolbarComponent: DevicesToolbar,
      },

    },
    {
      path: "/switchgears",
      name: "switchgears",
      component: SwitchgearView,
      meta: {
        ...defaultMeta,
        toolbarComponent: SwitchgearsToolbar,
      },
    },
    {
      path: "/signal-list",
      name: "signal-list",
      component: SignalListView,
      meta: {
        ...defaultMeta,
        toolbarComponent: SignalListToolbar,
      },
    },
    {
      path: "/sequences",
      name: "sequences",
      component: SequencesView,
      meta: {
        ...defaultMeta,
        toolbarComponent: SequencesToolbar,
      },
    },
    {
      path: "/settings",
      name: "settings",
      component: SettingsView,
      meta: {
        ...defaultMeta,
        toolbarComponent: SettingsToolbar,
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
        bottomAside: false,
        toolbarComponent: EventsToolbar,
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
