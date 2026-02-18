export type ProjectSignalDirection = "DI" | "DO" | "AI" | "AO"
export type SimulatorChannelType = "di" | "do" | "ai" | "ao"

export function resolveRuntimeChannelTypeForSignal(signalDirection: string): SimulatorChannelType | null {
  const normalized = signalDirection.trim().toUpperCase()

  if (normalized === "DI") return "do"
  if (normalized === "DO") return "di"
  if (normalized === "AI") return "ao"
  if (normalized === "AO") return "ai"

  return null
}
