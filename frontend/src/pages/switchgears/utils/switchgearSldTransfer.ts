import type { Channel } from "@/types/channel"
import type { Switchgear, SwitchgearBindingInput, SwitchgearBindingRole } from "@/types/switchgear"

import type { DiagramEdge, StoredDiagramState } from "./switchgearSldDiagramTypes"

export const SWITCHGEAR_SLD_TRANSFER_SCHEMA = "unitlab.sld.transfer.v1" as const

type TransferChannelRef = {
  unitId: string
  channelIndex: number
  channelType?: string
}

export type SwitchgearSldTransferBinding = {
  role: string
  delayMs: number
  channel: TransferChannelRef | null
}

export type SwitchgearSldTransferSwitchgear = {
  key: string
  name: string
  switchgearType: string
  bindings: SwitchgearSldTransferBinding[]
}

export type SwitchgearSldTransferPayload = {
  schema: typeof SWITCHGEAR_SLD_TRANSFER_SCHEMA
  kind: "full" | "selection"
  exportedAt: string
  sourceWorkspaceId?: number
  diagram: StoredDiagramState
  switchgears: SwitchgearSldTransferSwitchgear[]
}

export type SwitchgearSldTransferSource = {
  switchgears: ReadonlyArray<Switchgear>
  channels: ReadonlyArray<Channel>
  resolveUnitId: (deviceId: number) => string
}

export function createSwitchgearSldTransfer(
  state: StoredDiagramState,
  source: SwitchgearSldTransferSource,
  selectedIds?: ReadonlyArray<string>,
): SwitchgearSldTransferPayload {
  const isSelection = selectedIds !== undefined
  const selected = new Set(selectedIds ?? [])
  const allSwitchgearIds = source.switchgears.map(item => String(item.id))
  const selectedSwitchgearIds = new Set(
    (isSelection ? allSwitchgearIds.filter(id => selected.has(`switchgear:${id}`)) : allSwitchgearIds),
  )
  const sourceEdges = state.edges ?? state.lines ?? []
  const edges = isSelection
    ? sourceEdges.filter(edge => selected.has(edge.id))
    : sourceEdges
  const staticElements = isSelection
    ? (state.staticElements ?? []).filter(element => selected.has(element.id) || selected.has(`static:${element.id}`))
    : state.staticElements ?? []
  const textElements = isSelection
    ? (state.textElements ?? []).filter(element => selected.has(element.id))
    : state.textElements ?? []

  for (const edge of edges) {
    for (const binding of [edge.startBinding, edge.endBinding]) {
      if (binding?.ownerType === "node") {
        selectedSwitchgearIds.add(String(binding.ownerId))
      }
    }
  }

  const switchgearById = new Map(source.switchgears.map(item => [String(item.id), item]))
  const switchgears = [...selectedSwitchgearIds]
    .map(id => switchgearById.get(id))
    .filter((item): item is Switchgear => Boolean(item))
    .map(item => serializeSwitchgear(item, source.channels, source.resolveUnitId))

  const diagram: StoredDiagramState = {
    layoutById: filterRecord(state.layoutById, selectedSwitchgearIds, id => String(id)),
    labelOffsetById: filterRecord(state.labelOffsetById, selectedSwitchgearIds, id => String(id)),
    zIndexById: isSelection ? filterSelectionZIndexes(state.zIndexById, selected, selectedSwitchgearIds, staticElements, textElements, edges) : state.zIndexById,
    rotationById: isSelection ? filterSelectionZIndexes(state.rotationById, selected, selectedSwitchgearIds, staticElements, textElements, edges) : state.rotationById,
    edges: edges.map(edge => sanitizeEdgeBindings(edge, selectedSwitchgearIds, new Set(staticElements.map(item => item.id)))),
    staticElements,
    textElements,
    snapEnabled: state.snapEnabled,
    viewState: isSelection ? undefined : state.viewState,
  }

  return {
    schema: SWITCHGEAR_SLD_TRANSFER_SCHEMA,
    kind: isSelection ? "selection" : "full",
    exportedAt: new Date().toISOString(),
    sourceWorkspaceId: state.workspaceId,
    diagram,
    switchgears,
  }
}

function serializeSwitchgear(
  switchgear: Switchgear,
  channels: ReadonlyArray<Channel>,
  resolveUnitId: (deviceId: number) => string,
): SwitchgearSldTransferSwitchgear {
  return {
    key: `switchgear:${switchgear.id}`,
    name: switchgear.name,
    switchgearType: switchgear.switchgear_type,
    bindings: switchgear.bindings.map(binding => {
      const channel = binding.channel_id == null
        ? null
        : channels.find(item => item.id === binding.channel_id)
      return {
        role: binding.role,
        delayMs: binding.delay_ms ?? 0,
        channel: channel
          ? {
              unitId: resolveUnitId(channel.device_id),
              channelIndex: channel.index,
              channelType: channel.type,
            }
          : null,
      }
    }),
  }
}

function sanitizeEdgeBindings(edge: DiagramEdge, switchgearIds: ReadonlySet<string>, staticIds: ReadonlySet<string>): DiagramEdge {
  const validBinding = (binding: DiagramEdge["startBinding"]) => (
    binding?.ownerType === "node"
      ? (switchgearIds.has(String(binding.ownerId)) ? binding : null)
      : binding?.ownerType === "static" && staticIds.has(String(binding.ownerId))
        ? binding
        : null
  )
  return {
    ...edge,
    startBinding: validBinding(edge.startBinding),
    endBinding: validBinding(edge.endBinding),
  }
}

function filterRecord<T>(
  record: Record<string, T> | undefined,
  ids: ReadonlySet<string>,
  normalize: (id: string) => string,
): Record<string, T> | undefined {
  if (!record) return undefined
  return Object.fromEntries(Object.entries(record).filter(([id]) => ids.has(normalize(id))))
}

function filterSelectionZIndexes<T>(
  record: Record<string, T> | undefined,
  selected: ReadonlySet<string>,
  switchgearIds: ReadonlySet<string>,
  staticElements: ReadonlyArray<{ id: string }>,
  textElements: ReadonlyArray<{ id: string }>,
  edges: ReadonlyArray<{ id: string }>,
): Record<string, T> | undefined {
  if (!record) return undefined
  const allowed = new Set<string>([
    ...[...switchgearIds].map(id => `switchgear:${id}`),
    ...staticElements.flatMap(item => [item.id, `static:${item.id}`]),
    ...textElements.map(item => item.id),
    ...edges.map(item => item.id),
  ])
  return Object.fromEntries(Object.entries(record).filter(([id]) => allowed.has(id) && (selected.has(id) || switchgearIds.has(id.replace(/^switchgear:/, "")))))
}

export function parseSwitchgearSldTransfer(value: unknown): SwitchgearSldTransferPayload {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("Invalid SLD transfer file")
  }
  const payload = value as Partial<SwitchgearSldTransferPayload>
  if (payload.schema !== SWITCHGEAR_SLD_TRANSFER_SCHEMA || (payload.kind !== "full" && payload.kind !== "selection")) {
    throw new Error("Unsupported SLD transfer file")
  }
  if (!payload.diagram || typeof payload.diagram !== "object" || !Array.isArray(payload.switchgears)) {
    throw new Error("SLD transfer file is incomplete")
  }
  return payload as SwitchgearSldTransferPayload
}

export function resolveImportedBindings(
  imported: SwitchgearSldTransferSwitchgear,
  channels: ReadonlyArray<Channel>,
  resolveUnitId: (deviceId: number) => string,
): { bindings: SwitchgearBindingInput[]; detachedRoles: string[] } {
  const bindings: SwitchgearBindingInput[] = []
  const detachedRoles: string[] = []
  for (const binding of imported.bindings) {
    const channel = binding.channel
      ? channels.find(item => (
        resolveUnitId(item.device_id) === binding.channel?.unitId
        && item.index === binding.channel.channelIndex
        && (!binding.channel.channelType || item.type === binding.channel.channelType)
      ))
      : null
    if (binding.channel && !channel) detachedRoles.push(binding.role)
    bindings.push({
      role: binding.role as SwitchgearBindingRole,
      channel_id: channel?.id ?? null,
      delay_ms: binding.delayMs,
    })
  }
  return { bindings, detachedRoles }
}
