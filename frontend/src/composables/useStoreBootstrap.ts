type BootstrapTask = () => Promise<unknown> | unknown

const inFlightByKey = new Map<string, Promise<void>>()

function normalizeKeyParts(parts: readonly (string | number | null | undefined)[]): string {
  return parts.map(part => String(part ?? "").trim()).join("::")
}

async function runTasks(tasks: readonly BootstrapTask[], mode: "strict" | "settled"): Promise<void> {
  if (tasks.length === 0) {
    return
  }
  if (mode === "settled") {
    await Promise.allSettled(tasks.map(task => Promise.resolve().then(task)))
    return
  }
  await Promise.all(tasks.map(task => Promise.resolve().then(task)))
}

export async function runStoreBootstrap(
  keyParts: readonly (string | number | null | undefined)[],
  tasks: readonly BootstrapTask[],
  options: { mode?: "strict" | "settled" } = {},
): Promise<void> {
  const key = normalizeKeyParts(keyParts)
  const existing = inFlightByKey.get(key)
  if (existing) {
    await existing
    return
  }

  const task = Promise.resolve().then(() =>
    runTasks(tasks, options.mode ?? "strict"),
  )
  inFlightByKey.set(key, task)
  try {
    await task
  } finally {
    if (inFlightByKey.get(key) === task) {
      inFlightByKey.delete(key)
    }
  }
}
