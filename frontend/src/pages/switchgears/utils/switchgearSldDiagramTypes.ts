export type DiagramNodeLayout = {
  x: number
  y: number
}

export type DiagramBindablePortOwnerType = "node" | "static"

export type DiagramPortBinding = {
  ownerType: DiagramBindablePortOwnerType
  ownerId: number | string
  portId: string
}

export type DiagramEdge = {
  id: string
  x1: number
  y1: number
  x2: number
  y2: number
  kind: "line" | "arrow"
  weight?: "normal" | "bold"
  startBinding?: DiagramPortBinding | null
  endBinding?: DiagramPortBinding | null
}

export type DiagramStaticKind = "transformer" | "ground"

export type DiagramStaticSize = "sm" | "md" | "lg"

export type DiagramTextSize = "md"

export type DiagramStaticElement = {
  id: string
  kind: DiagramStaticKind
  size: DiagramStaticSize
  x: number
  y: number
  rotation: 0 | 90 | 180 | 270
}

export type DiagramTextElement = {
  id: string
  text: string
  size: DiagramTextSize
  x: number
  y: number
}

export type DiagramViewState = {
  x: number
  y: number
  zoom: number
}

export type StoredDiagramState = {
  workspaceId?: number
  layoutById?: Record<string, DiagramNodeLayout>
  labelOffsetById?: Record<string, { x: number; y: number }>
  edges?: DiagramEdge[]
  lines?: DiagramEdge[]
  staticElements?: DiagramStaticElement[]
  textElements?: DiagramTextElement[]
  snapEnabled?: boolean
  viewState?: DiagramViewState
}
