import { logger } from "@/utils/logger"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useSequenceStore } from "@/stores/sequenceStore"

let booted = false

export async function bootRuntime() {
  if (booted) return
  booted = true

  logger.info("🧠 Boot: Runtime…")

  const workspaceStore = useWorkspaceStore()
  const deviceStore = useDeviceStore()
  const switchgearStore = useSwitchgearStore()
  const sequenceStore = useSequenceStore()

  await workspaceStore.bootstrap()
  await deviceStore.ensureLoaded()
  await switchgearStore.ensureLoaded()
  await sequenceStore.ensureLoaded()

  logger.info("✅ Boot: Runtime ready")
}
