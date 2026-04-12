import { CHANNEL_TYPES, type AoChannel, type AoChannelDiagnostics, type Channel, type ChannelDiagnostics, type DiChannel, type DoChannel } from "@/types/channel"

function channelBit(index: number): number {
  return 2 ** index
}

export type DoDiagnosticsBitmasks = {
  open_mask: number
  fault_mask: number
  soft_mask: number
}

export type DiDiagnosticsBitmasks = Partial<{
  seen_mask: number
  stuck_mask: number
  lost_mask: number
  latched_mask: number
  latched_changed_mask: number
  latched_cause_mask: number
}>

export type AoDiagnosticsBitmasks = {
  valid_mask: number
  pending_mask: number
  fault_mask: number
  error_mask: number
}

export type DiDiagnosticField =
  | "seen"
  | "stuck"
  | "lost"
  | "latched"
  | "latchedChanged"
  | "latchedCause"

export type DiDiagnosticsChange = {
  channel: DiChannel
  updates: Array<{ field: DiDiagnosticField; value: boolean }>
}

export type AoDiagnosticsChange = {
  channel: AoChannel
  previousQuality?: AoChannelDiagnostics["quality"]
  nextQuality: AoChannelDiagnostics["quality"]
  previousHasError: boolean
  nextHasError: boolean
}

export const DI_DIAG_LABELS: Record<DiDiagnosticField, { on: string; off: string; alert?: boolean; log?: boolean }> = {
  seen: { on: "activity detected", off: "activity reset" },
  stuck: { on: "channel stuck", off: "stuck cleared", alert: true },
  lost: { on: "signal lost", off: "signal restored", alert: true },
  latched: { on: "latched", off: "latch cleared" },
  latchedChanged: { on: "latchedΔ changed", off: "latchedΔ cleared", log: false },
  latchedCause: { on: "cause updated", off: "cause cleared", log: false },
}

export function ensureDoDiagnostics(channel: DoChannel): ChannelDiagnostics {
  if (!channel.diagnostics) {
    channel.diagnostics = { open: false, fault: false, soft: false }
  }
  return channel.diagnostics
}

export function ensureDiDiagnostics(channel: DiChannel) {
  if (!channel.diDiagnostics) {
    channel.diDiagnostics = {
      seen: false,
      stuck: false,
      lost: false,
      latched: false,
      latchedChanged: false,
      latchedCause: false,
    }
  }
  return channel.diDiagnostics
}

export function ensureAoDiagnostics(channel: AoChannel): AoChannelDiagnostics {
  if (!channel.diagnostics) {
    channel.diagnostics = { quality: "fault", hasError: false }
  }
  return channel.diagnostics
}

export function applyDoDiagnostics(doChannels: DoChannel[], diag: DoDiagnosticsBitmasks): boolean {
  if (!doChannels.length) {
    return false
  }

  let changed = false
  doChannels.forEach(ch => {
    const state = ensureDoDiagnostics(ch)
    const nextOpen = ((diag.open_mask >>> ch.index) & 1) === 1
    const nextFault = ((diag.fault_mask >>> ch.index) & 1) === 1
    const nextSoft = ((diag.soft_mask >>> ch.index) & 1) === 1
    if (state.open !== nextOpen || state.fault !== nextFault || state.soft !== nextSoft) {
      state.open = nextOpen
      state.fault = nextFault
      state.soft = nextSoft
      changed = true
    }
  })
  return changed
}

export function applyAoDiagnostics(aoChannels: AoChannel[], diag: AoDiagnosticsBitmasks): AoDiagnosticsChange[] {
  if (!aoChannels.length) {
    return []
  }

  const changes: AoDiagnosticsChange[] = []
  aoChannels.forEach(ch => {
    const bit = channelBit(ch.index)
    const nextHasError = (diag.error_mask & bit) !== 0
    const inValid = (diag.valid_mask & bit) !== 0
    const inPending = (diag.pending_mask & bit) !== 0
    const inFault = (diag.fault_mask & bit) !== 0

    const state = ensureAoDiagnostics(ch)
    const previousQuality = state.quality
    const previousHasError = state.hasError

    let nextQuality = previousQuality
    if (inFault) {
      nextQuality = "fault"
    } else if (inPending) {
      nextQuality = "pending"
    } else if (inValid) {
      nextQuality = "valid"
    }

    if (previousQuality !== nextQuality || previousHasError !== nextHasError) {
      state.quality = nextQuality
      state.hasError = nextHasError
      changes.push({
        channel: ch,
        previousQuality,
        nextQuality,
        previousHasError,
        nextHasError,
      })
    }
  })

  return changes
}

export function applyDiDiagnostics(diChannels: DiChannel[], diag: DiDiagnosticsBitmasks): DiDiagnosticsChange[] {
  if (!diChannels.length) {
    return []
  }

  const changes: DiDiagnosticsChange[] = []
  diChannels.forEach(ch => {
    const state = ensureDiDiagnostics(ch)
    const updates: DiDiagnosticsChange["updates"] = []

    if (diag.seen_mask !== undefined) {
      const next = ((diag.seen_mask >>> ch.index) & 1) === 1
      if (state.seen !== next) {
        state.seen = next
        updates.push({ field: "seen", value: next })
      }
    }

    if (diag.stuck_mask !== undefined) {
      const next = ((diag.stuck_mask >>> ch.index) & 1) === 1
      if (state.stuck !== next) {
        state.stuck = next
        updates.push({ field: "stuck", value: next })
      }
    }

    if (diag.lost_mask !== undefined) {
      const next = ((diag.lost_mask >>> ch.index) & 1) === 1
      if (state.lost !== next) {
        state.lost = next
        updates.push({ field: "lost", value: next })
      }
    }

    if (diag.latched_mask !== undefined) {
      const next = ((diag.latched_mask >>> ch.index) & 1) === 1
      if (state.latched !== next) {
        state.latched = next
        updates.push({ field: "latched", value: next })
      }
    }

    if (diag.latched_changed_mask !== undefined) {
      const next = ((diag.latched_changed_mask >>> ch.index) & 1) === 1
      if (state.latchedChanged !== next) {
        state.latchedChanged = next
        updates.push({ field: "latchedChanged", value: next })
      }
    }

    if (diag.latched_cause_mask !== undefined) {
      const next = ((diag.latched_cause_mask >>> ch.index) & 1) === 1
      if (state.latchedCause !== next) {
        state.latchedCause = next
        updates.push({ field: "latchedCause", value: next })
      }
    }

    if (updates.length) {
      changes.push({ channel: ch, updates })
    }
  })

  return changes
}

export type BitDeltaChange = { channel: Channel; value: boolean }

export function applyDeltaState(
  channels: Channel[],
  deviceId: number,
  changedMask: number,
  stateMask: number,
  options?: { onDoUpdate?: (channel: DoChannel, next: boolean) => void },
): { changed: boolean; actionIds: Set<string>; updates: BitDeltaChange[] } {
  let changed = false
  const actionIds = new Set<string>()
  const updates: BitDeltaChange[] = []
  channels.forEach(ch => {
    if (ch.device_id !== deviceId || ch.type === CHANNEL_TYPES.AO) {
      return
    }
    const bit = channelBit(ch.index)
    if ((changedMask & bit) === 0) {
      return
    }
    const next = (stateMask & bit) !== 0
    if (ch.state !== next) {
      ch.state = next
      changed = true
    }
    if (ch.type === CHANNEL_TYPES.DO) {
      const id = ch.ui?.actionId
      if (id) {
        actionIds.add(id)
      }
      options?.onDoUpdate?.(ch as DoChannel, next)
    }
    updates.push({ channel: ch, value: next })
  })
  return { changed, actionIds, updates }
}

export function describeDiDiagnosticUpdates(updates: DiDiagnosticsChange["updates"]) {
  const labels: string[] = []
  let alert = false
  updates.forEach(({ field, value }) => {
    const meta = DI_DIAG_LABELS[field]
    if (!meta || meta.log === false) {
      return
    }
    labels.push(value ? meta.on : meta.off)
    if (value && meta.alert) {
      alert = true
    }
  })
  return { labels, alert }
}
