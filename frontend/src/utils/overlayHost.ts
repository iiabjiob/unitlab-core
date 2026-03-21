export const APP_OVERLAY_HOST_ID = "affino-overlay-host"
export const APP_OVERLAY_HOST_SELECTOR = `#${APP_OVERLAY_HOST_ID}`

export function ensureAppOverlayHost(): string {
  if (typeof document === "undefined") {
    return APP_OVERLAY_HOST_SELECTOR
  }

  let host = document.getElementById(APP_OVERLAY_HOST_ID)
  if (!host) {
    host = document.createElement("div")
    host.id = APP_OVERLAY_HOST_ID
    host.setAttribute("data-unitlab-overlay-host", "true")
    document.body.appendChild(host)
  }

  return APP_OVERLAY_HOST_SELECTOR
}