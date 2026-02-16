import { logger } from "@/utils/logger"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"

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

    const workspaceStore = useWorkspaceStore()
    const deviceStore = useDeviceStore()
    const switchgearStore = useSwitchgearStore()
    const sequenceStore = useSequenceStore()
    const signalSheetStore = useSignalSheetStore()

    const workspaceReady = await workspaceStore.bootstrap()
    if (!workspaceReady) {
      logger.warn("⚠️ Boot: workspace bootstrap failed, will retry on next navigation")
      return
    }

    await Promise.all([
      deviceStore.ensureLoaded(),
      switchgearStore.ensureLoaded(),
      sequenceStore.ensureLoaded(),
      signalSheetStore.refreshSheet().catch((error: unknown) => {
        logger.warn("⚠️ Boot: signal sheet probe failed (welcome recommendation may be stale)", error)
      }),
    ])

    booted = true
    logger.info("✅ Boot: Runtime ready")
  })()

  try {
    await bootInFlight
  } finally {
    bootInFlight = null
  }
}
