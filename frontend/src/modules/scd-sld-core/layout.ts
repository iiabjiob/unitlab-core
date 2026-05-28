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
const SCD_COORDINATE_STEP_UNITS = 2
const FALLBACK_BAY_WIDTH_UNITS = 10
const FALLBACK_NODE_Y_STEP_UNITS = 3
const LANE_HEIGHT_UNITS = 36
const UNGROUPED_OFFSET_UNITS = 3
const BUSBAR_WIDTH_UNITS = 6
const BUSBAR_HEIGHT_UNITS = 1
const BUSBAR_SPAN_PADDING_UNITS = 4

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

  const positionedElements = baseDocument.elements.map((element) => {
    const position = positionsBySourceId.get(element.sourceId) ?? snapCoordinate(element.position, gridSize)
    return {
      ...element,
      position,
      visual: withLayoutVisualDimensions(element.visual, gridSize),
    }
  })
  const elements = layoutBusbarSpans(positionedElements, baseDocument.connections, gridSize)
  const elementPositionsBySourceId = buildElementPositionsBySourceId(elements)
  const elementsBySourceId = new Map(elements.map(element => [element.sourceId, element]))

  return {
    ...baseDocument,
    elements,
    connections: baseDocument.connections.map(connection => ({
      ...connection,
      route: buildConnectionRoute(connection, elementPositionsBySourceId, elementsBySourceId, gridSize),
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
    const laneBase = resolveLaneOrigin(lane.position, lane.orderIndex, gridSize)

    for (const bayCell of lane.bayCells) {
      const bayOrigin = resolveBayOrigin(laneBase, bayCell.position, bayCell.orderIndex, gridSize)
      for (const node of bayCell.nodes) {
        positions.set(node.sourceId, resolveNodePosition(bayOrigin, node.position, node.orderIndex, gridSize))
      }
    }

    const ungroupedX = toGridCoordinate(
      ORIGIN_X_UNITS + lane.bayCells.length * FALLBACK_BAY_WIDTH_UNITS + UNGROUPED_OFFSET_UNITS,
      gridSize,
    )
    for (const node of lane.ungroupedNodes) {
      positions.set(node.sourceId, resolveNodePosition(
        { x: ungroupedX, y: laneBase.y },
        node.position,
        node.orderIndex,
        gridSize,
      ))
    }
  }

  const orphanY = toGridCoordinate(
    ORIGIN_Y_UNITS + cellModel.voltageLevels.length * LANE_HEIGHT_UNITS + FALLBACK_NODE_Y_STEP_UNITS,
    gridSize,
  )
  for (const node of cellModel.orphanNodes) {
    positions.set(node.sourceId, hasCoordinate(node.position.x) || hasCoordinate(node.position.y)
      ? {
          x: toGridCoordinate(ORIGIN_X_UNITS, gridSize) + coordinateOffset(node.position.x, gridSize),
          y: toGridCoordinate(ORIGIN_Y_UNITS, gridSize) + coordinateOffset(node.position.y, gridSize),
        }
      : {
          x: toGridCoordinate(ORIGIN_X_UNITS, gridSize),
          y: orphanY + toGridCoordinate(node.orderIndex * FALLBACK_NODE_Y_STEP_UNITS, gridSize),
        })
  }

  return positions
}

function resolveLaneOrigin(position: SldCoordinate, orderIndex: number, gridSize: number): SldRoutePoint {
  return {
    x: toGridCoordinate(ORIGIN_X_UNITS, gridSize) + coordinateOffset(position.x, gridSize),
    y: toGridCoordinate(ORIGIN_Y_UNITS + orderIndex * LANE_HEIGHT_UNITS, gridSize) + coordinateOffset(position.y, gridSize),
  }
}

function resolveBayOrigin(
  laneBase: SldRoutePoint,
  position: SldCoordinate,
  orderIndex: number,
  gridSize: number,
): SldRoutePoint {
  return {
    x: laneBase.x + (
      hasCoordinate(position.x)
        ? coordinateOffset(position.x, gridSize)
        : toGridCoordinate(orderIndex * FALLBACK_BAY_WIDTH_UNITS, gridSize)
    ),
    y: laneBase.y + coordinateOffset(position.y, gridSize),
  }
}

function resolveNodePosition(
  bayOrigin: SldRoutePoint,
  position: SldCoordinate,
  orderIndex: number,
  gridSize: number,
): SldCoordinate {
  return {
    x: bayOrigin.x + coordinateOffset(position.x, gridSize),
    y: bayOrigin.y + (
      hasCoordinate(position.y)
        ? coordinateOffset(position.y, gridSize)
        : toGridCoordinate(orderIndex * FALLBACK_NODE_Y_STEP_UNITS, gridSize)
    ),
  }
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

function layoutBusbarSpans(elements: SldElement[], connections: SldConnection[], gridSize: number): SldElement[] {
  const positionsBySourceId = buildElementPositionsBySourceId(elements)
  const elementsBySourceId = new Map(elements.map(element => [element.sourceId, element]))

  return elements.map((element) => {
    if (element.visual.representation !== "busbar" || element.position.x === null || element.position.y === null) {
      return element
    }

    const connectedPoints = connections
      .filter(connection => connection.terminalOwnerIds.includes(element.sourceId))
      .flatMap(connection => connection.terminalOwnerIds)
      .filter(sourceId => sourceId !== element.sourceId)
      .filter(sourceId => elementsBySourceId.get(sourceId)?.visual.representation !== "busbar")
      .map(sourceId => positionsBySourceId.get(sourceId))
      .filter((point): point is SldRoutePoint => point !== undefined)

    if (connectedPoints.length === 0) {
      return element
    }

    const minX = Math.min(element.position.x, ...connectedPoints.map(point => point.x))
    const maxX = Math.max(element.position.x, ...connectedPoints.map(point => point.x))
    const width = Math.max(
      toGridCoordinate(BUSBAR_WIDTH_UNITS, gridSize),
      snapNumber(maxX - minX + toGridCoordinate(BUSBAR_SPAN_PADDING_UNITS, gridSize), gridSize),
    )

    return {
      ...element,
      position: {
        x: snapNumber((minX + maxX) / 2, gridSize),
        y: element.position.y,
      },
      visual: {
        ...element.visual,
        orientation: "horizontal",
        dimensions: {
          width,
          height: toGridCoordinate(BUSBAR_HEIGHT_UNITS, gridSize),
        },
      },
    }
  })
}

function buildConnectionRoute(
  connection: SldConnection,
  positionsBySourceId: Map<string, SldRoutePoint>,
  elementsBySourceId: Map<string, SldElement>,
  gridSize: number,
): SldConnectionRoute | null {
  const endpointPositions = connection.terminalOwnerIds
    .map(terminalOwnerId => ({
      terminalOwnerId,
      point: positionsBySourceId.get(terminalOwnerId) ?? null,
      element: elementsBySourceId.get(terminalOwnerId) ?? null,
    }))
    .filter((item): item is { terminalOwnerId: string; point: SldRoutePoint; element: SldElement | null } => item.point !== null)

  if (endpointPositions.length < 2) {
    return null
  }

  const busbarEndpoint = endpointPositions.find(item => item.element?.visual.representation === "busbar")
  if (busbarEndpoint) {
    const nonBusbarEndpoints = endpointPositions.filter(item => item.terminalOwnerId !== busbarEndpoint.terminalOwnerId)
    if (nonBusbarEndpoints.length > 0) {
      return {
        kind: "orthogonal-star",
        anchor: busbarEndpoint.point,
        segments: nonBusbarEndpoints.map(({ terminalOwnerId, point }) => ({
          terminalOwnerId,
          points: uniqueConsecutivePoints([
            point,
            { x: point.x, y: busbarEndpoint.point.y },
          ]),
        })),
      }
    }
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

function coordinateOffset(value: number | null, gridSize: number): number {
  return hasCoordinate(value)
    ? toGridCoordinate(value * SCD_COORDINATE_STEP_UNITS, gridSize)
    : 0
}

function hasCoordinate(value: number | null): value is number {
  return Number.isFinite(value)
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
