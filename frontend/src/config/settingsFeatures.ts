const serviceModeRaw = String(import.meta.env.VITE_SETTINGS_SERVICE_MODE ?? "").trim().toLowerCase()

export const SETTINGS_SERVICE_MODE_ENABLED =
  serviceModeRaw === "1" || serviceModeRaw === "true" || serviceModeRaw === "yes" || serviceModeRaw === "on"

