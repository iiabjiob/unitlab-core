export default [
  {
    path: "/settings",
    component: () => import("@/views/settings/SettingsView.vue"),
    children: [
      {
        path: "wifi",
        component: () => import("@/views/settings/WifiSettings.vue"),
      },
      {
        path: "other",
        component: () => import("@/views/settings/OtherSettings.vue"),
      },
    ],
  },
];
