import type { SignalGridPatchQueueFlushOptions } from "@/pages/signals/composables/useSignalGridPatchQueue"

type SignalGridPatchMode = "row" | "refresh"

type SignalGridColumnPatchPolicy = {
  mode: SignalGridPatchMode
  recomputeSort: boolean
  recomputeFilter: boolean
  recomputeGroup: boolean
}

type SignalGridPatchPolicyOptions = SignalGridPatchQueueFlushOptions & {
  columns?: readonly string[]
  flush?: boolean
}

type ResolvedSignalGridPatchPolicy = SignalGridPatchQueueFlushOptions & {
  mode: SignalGridPatchMode
}

const DEFAULT_ROW_PATCH_POLICY: SignalGridColumnPatchPolicy = {
  mode: "row",
  recomputeSort: false,
  recomputeFilter: false,
  recomputeGroup: false,
}

const COLUMN_PATCH_POLICIES: Record<string, SignalGridColumnPatchPolicy> = {
  internal_signal_type: DEFAULT_ROW_PATCH_POLICY,
  channel_select: {
    mode: "row",
    recomputeSort: false,
    recomputeFilter: true,
    recomputeGroup: false,
  },
  allocation_status: {
    mode: "row",
    recomputeSort: true,
    recomputeFilter: true,
    recomputeGroup: true,
  },
  allocation_health: {
    mode: "row",
    recomputeSort: true,
    recomputeFilter: true,
    recomputeGroup: true,
  },
  tested_at: DEFAULT_ROW_PATCH_POLICY,
  runtime_status: {
    mode: "refresh",
    recomputeSort: false,
    recomputeFilter: false,
    recomputeGroup: false,
  },
  runtime_value: {
    mode: "refresh",
    recomputeSort: false,
    recomputeFilter: false,
    recomputeGroup: false,
  },
  runtime_updated_at: {
    mode: "refresh",
    recomputeSort: false,
    recomputeFilter: false,
    recomputeGroup: false,
  },
  runtime_reason: {
    mode: "refresh",
    recomputeSort: false,
    recomputeFilter: false,
    recomputeGroup: false,
  },
}

function normalizeColumnKey(column: unknown): string {
  return String(column ?? "").trim()
}

function resolveSignalGridColumnPatchPolicy(column: unknown): SignalGridColumnPatchPolicy {
  const key = normalizeColumnKey(column)
  if (!key) {
    return DEFAULT_ROW_PATCH_POLICY
  }
  if (key.startsWith("source_col_")) {
    return DEFAULT_ROW_PATCH_POLICY
  }
  return COLUMN_PATCH_POLICIES[key] ?? DEFAULT_ROW_PATCH_POLICY
}

export function resolveSignalGridPatchPolicy(
  columns: readonly string[] | undefined,
  options?: SignalGridPatchPolicyOptions,
): ResolvedSignalGridPatchPolicy {
  const columnPolicies = (columns && columns.length > 0)
    ? columns.map(resolveSignalGridColumnPatchPolicy)
    : [DEFAULT_ROW_PATCH_POLICY]

  const policy = columnPolicies.reduce<SignalGridColumnPatchPolicy>((merged, item) => ({
    mode: merged.mode === "row" || item.mode === "row" ? "row" : "refresh",
    recomputeSort: merged.recomputeSort || item.recomputeSort,
    recomputeFilter: merged.recomputeFilter || item.recomputeFilter,
    recomputeGroup: merged.recomputeGroup || item.recomputeGroup,
  }), {
    mode: "refresh",
    recomputeSort: false,
    recomputeFilter: false,
    recomputeGroup: false,
  })

  return {
    mode: policy.mode,
    reason: options?.reason,
    recomputeSort: options?.recomputeSort ?? policy.recomputeSort,
    recomputeFilter: options?.recomputeFilter ?? policy.recomputeFilter,
    recomputeGroup: options?.recomputeGroup ?? policy.recomputeGroup,
    emit: options?.emit,
    immediate: options?.immediate,
  }
}
