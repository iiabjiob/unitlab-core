import { logger } from "@/utils/logger"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { pinia } from "@/stores/pinia"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

let booted = false
let bootInFlight: Promise<void> | null = null

export async function bootRuntime() {
  if (booted) return
  if (bootInFlight) {
    await bootInFlight
    return
  }

  bootInFlight = (async () => {
    logger.info("🧠 Boot: Runtime…")

    const workspaceStore = useWorkspaceStore(pinia)
    const deviceStore = useDeviceStore(pinia)
    const switchgearStore = useSwitchgearStore(pinia)
    const sequenceStore = useSequenceStore(pinia)

    const workspaceReady = await workspaceStore.bootstrap()
    if (!workspaceReady) {
      logger.warn("⚠️ Boot: workspace bootstrap failed, will retry on next navigation")
      return
    }

    await runStoreBootstrap(
      ["boot-runtime", workspaceStore.activeWorkspaceId],
      [
        () => deviceStore.ensureLoaded(),
        () => switchgearStore.ensureLoaded(),
        () => sequenceStore.ensureLoaded(),
      ],
      { mode: "strict" },
    )

    booted = true
    logger.info("✅ Boot: Runtime ready")
  })()

  try {
    await bootInFlight
  } finally {
    bootInFlight = null
  }
}
