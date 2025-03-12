import { defineAsyncComponent } from "vue";

export default [
  {
    path: "/settings",
    component: defineAsyncComponent(() => import("@/views/settings/SettingsView.vue")),
    children: [
      {
        path: "wifi",
        component: defineAsyncComponent(() => import("@/views/settings/WifiSettings.vue")),
      },
      {
        path: "other",
        component: defineAsyncComponent(() => import("@/views/settings/OtherSettings.vue")),
      },

    ],
  },
];
