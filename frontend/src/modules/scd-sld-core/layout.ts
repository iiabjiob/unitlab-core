import { createSldDocumentFromGraph } from "./sldDocument"
import type {
  ElectricalGraph,
  GenerateSldOptions,
  SldCellModel,
  SldConnection,
  SldConnectionRoute,
  SldCoordinate,
  SldDocument,
  SldElement,
  SldElementVisual,
  SldRoutePoint,
} from "./types"

const DEFAULT_GRID_SIZE = 24
const ORIGIN_X_UNITS = 4
const ORIGIN_Y_UNITS = 3
const BAY_WIDTH_UNITS = 8
const NODE_X_OFFSET_UNITS = 3
const NODE_Y_OFFSET_UNITS = 2
const NODE_Y_STEP_UNITS = 2
const LANE_HEIGHT_UNITS = 14
const UNGROUPED_OFFSET_UNITS = 3
const BUSBAR_WIDTH_UNITS = 6
const BUSBAR_HEIGHT_UNITS = 1

export function layoutSldDocument(
  cellModel: SldCellModel,
  graph: ElectricalGraph,
  options: GenerateSldOptions = {},
): SldDocument {
  const gridSize = normalizeGridSize(options.gridSize)
  const baseDocument = createSldDocumentFromGraph(graph, {
    ...options,
    gridSize,
  })
  const positionsBySourceId = buildPositionsBySourceId(cellModel, gridSize)

  const elements = baseDocument.elements.map((element) => {
    const position = positionsBySourceId.get(element.sourceId) ?? snapCoordinate(element.position, gridSize)
    return {
      ...element,
      position,
      visual: withLayoutVisualDimensions(element.visual, gridSize),
    }
  })
  const elementPositionsBySourceId = buildElementPositionsBySourceId(elements)

  return {
    ...baseDocument,
    elements,
    connections: baseDocument.connections.map(connection => ({
      ...connection,
      route: buildConnectionRoute(connection, elementPositionsBySourceId, gridSize),
    })),
    labels: elements.map(element => ({
      id: `${element.id}/label`,
      sourceId: element.sourceId,
      text: element.label,
      position: element.position,
    })),
    diagnostics: [...cellModel.diagnostics],
    layoutHints: {
      generatedFrom: "scd",
      gridSize,
    },
  }
}

function buildPositionsBySourceId(cellModel: SldCellModel, gridSize: number): Map<string, SldCoordinate> {
  const positions = new Map<string, SldCoordinate>()

  for (const lane of cellModel.voltageLevels) {
    const laneY = toGridCoordinate(ORIGIN_Y_UNITS + lane.orderIndex * LANE_HEIGHT_UNITS, gridSize)

    for (const bayCell of lane.bayCells) {
      const bayX = toGridCoordinate(ORIGIN_X_UNITS + bayCell.orderIndex * BAY_WIDTH_UNITS, gridSize)
      for (const node of bayCell.nodes) {
        positions.set(node.sourceId, {
          x: bayX + toGridCoordinate(NODE_X_OFFSET_UNITS, gridSize),
          y: laneY + toGridCoordinate(NODE_Y_OFFSET_UNITS + node.orderIndex * NODE_Y_STEP_UNITS, gridSize),
        })
      }
    }

    const ungroupedX = toGridCoordinate(
      ORIGIN_X_UNITS + lane.bayCells.length * BAY_WIDTH_UNITS + UNGROUPED_OFFSET_UNITS,
      gridSize,
    )
    for (const node of lane.ungroupedNodes) {
      positions.set(node.sourceId, {
        x: ungroupedX,
        y: laneY + toGridCoordinate(NODE_Y_OFFSET_UNITS + node.orderIndex * NODE_Y_STEP_UNITS, gridSize),
      })
    }
  }

  const orphanY = toGridCoordinate(
    ORIGIN_Y_UNITS + cellModel.voltageLevels.length * LANE_HEIGHT_UNITS + NODE_Y_OFFSET_UNITS,
    gridSize,
  )
  for (const node of cellModel.orphanNodes) {
    positions.set(node.sourceId, {
      x: toGridCoordinate(ORIGIN_X_UNITS, gridSize),
      y: orphanY + toGridCoordinate(node.orderIndex * NODE_Y_STEP_UNITS, gridSize),
    })
  }

  return positions
}

function buildElementPositionsBySourceId(elements: SldElement[]): Map<string, SldRoutePoint> {
  const positions = new Map<string, SldRoutePoint>()

  for (const element of elements) {
    if (element.position.x === null || element.position.y === null) {
      continue
    }
    positions.set(element.sourceId, {
      x: element.position.x,
      y: element.position.y,
    })
  }

  return positions
}

function withLayoutVisualDimensions(visual: SldElementVisual, gridSize: number): SldElementVisual {
  if (visual.representation !== "busbar") {
    return visual
  }

  return {
    ...visual,
    dimensions: {
      width: toGridCoordinate(BUSBAR_WIDTH_UNITS, gridSize),
      height: toGridCoordinate(BUSBAR_HEIGHT_UNITS, gridSize),
    },
  }
}

function buildConnectionRoute(
  connection: SldConnection,
  positionsBySourceId: Map<string, SldRoutePoint>,
  gridSize: number,
): SldConnectionRoute | null {
  const endpointPositions = connection.terminalOwnerIds
    .map(terminalOwnerId => ({
      terminalOwnerId,
      point: positionsBySourceId.get(terminalOwnerId) ?? null,
    }))
    .filter((item): item is { terminalOwnerId: string; point: SldRoutePoint } => item.point !== null)

  if (endpointPositions.length < 2) {
    return null
  }

  const anchor = {
    x: snapNumber(average(endpointPositions.map(item => item.point.x)), gridSize),
    y: snapNumber(average(endpointPositions.map(item => item.point.y)), gridSize),
  }

  return {
    kind: "orthogonal-star",
    anchor,
    segments: endpointPositions.map(({ terminalOwnerId, point }) => ({
      terminalOwnerId,
      points: uniqueConsecutivePoints([
        point,
        { x: anchor.x, y: point.y },
        anchor,
      ]),
    })),
  }
}

function snapCoordinate(position: SldCoordinate, gridSize: number): SldCoordinate {
  return {
    x: position.x === null ? null : snapNumber(position.x, gridSize),
    y: position.y === null ? null : snapNumber(position.y, gridSize),
  }
}

function snapNumber(value: number, gridSize: number): number {
  return Math.round(value / gridSize) * gridSize
}

function average(values: number[]): number {
  return values.reduce((sum, value) => sum + value, 0) / values.length
}

function uniqueConsecutivePoints(points: SldRoutePoint[]): SldRoutePoint[] {
  return points.filter((point, index) => {
    const previous = points[index - 1]
    return !previous || previous.x !== point.x || previous.y !== point.y
  })
}

function toGridCoordinate(gridUnits: number, gridSize: number): number {
  return gridUnits * gridSize
}

function normalizeGridSize(gridSize: number | undefined): number {
  if (!gridSize || !Number.isFinite(gridSize) || gridSize <= 0) {
    return DEFAULT_GRID_SIZE
  }
  return gridSize
}
