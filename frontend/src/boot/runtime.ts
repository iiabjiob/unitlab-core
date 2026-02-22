import { logger } from "@/utils/logger"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { pinia } from "@/stores/pinia"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import { devPerfIncrement, devPerfMeasureStart } from "@/utils/devPerf"

let booted = false
let bootInFlight: Promise<void> | null = null

export async function bootRuntime() {
  devPerfIncrement("bootRuntime.calls")
  if (booted) return
  if (bootInFlight) {
    devPerfIncrement("bootRuntime.dedupe_waits")
    await bootInFlight
    return
  }

  bootInFlight = (async () => {
    const endBootRuntimeMeasure = devPerfMeasureStart("boot.runtime.total")
    logger.info("🧠 Boot: Runtime…")

    const workspaceStore = useWorkspaceStore(pinia)
    const deviceStore = useDeviceStore(pinia)
    const switchgearStore = useSwitchgearStore(pinia)
    const sequenceStore = useSequenceStore(pinia)

    const endWorkspaceBootstrapMeasure = devPerfMeasureStart("boot.runtime.workspaceBootstrap")
    const workspaceReady = await workspaceStore.bootstrap()
    endWorkspaceBootstrapMeasure({ ok: workspaceReady })
    if (!workspaceReady) {
      devPerfIncrement("bootRuntime.workspace_bootstrap_failed")
      logger.warn("⚠️ Boot: workspace bootstrap failed, will retry on next navigation")
      endBootRuntimeMeasure({ ok: false, reason: "workspace-bootstrap-failed" })
      return
    }

    const endRuntimeStoresBootstrapMeasure = devPerfMeasureStart("boot.runtime.storeBootstrap")
    await runStoreBootstrap(
      ["boot-runtime", workspaceStore.activeWorkspaceId],
      [
        () => deviceStore.ensureLoaded(),
        () => switchgearStore.ensureLoaded(),
        () => sequenceStore.ensureLoaded(),
      ],
      { mode: "strict" },
    )
    endRuntimeStoresBootstrapMeasure({
      workspaceId: workspaceStore.activeWorkspaceId ?? null,
    })

    booted = true
    devPerfIncrement("bootRuntime.completed")
    logger.info("✅ Boot: Runtime ready")
    endBootRuntimeMeasure({ ok: true })
  })()

  try {
    await bootInFlight
  } finally {
    bootInFlight = null
  }
}
