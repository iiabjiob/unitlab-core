import { createFlatSldDocumentFromGraph } from "./sldDocument"
import type {
  ElectricalGraph,
  GenerateSldOptions,
  SldBayCell,
  SldCellModel,
  SldCellNode,
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
const FEEDER_TEMPLATE_CENTER_X_UNITS = 5
const FEEDER_TEMPLATE_LEFT_X_UNITS = 0
const FEEDER_TEMPLATE_RIGHT_X_UNITS = 10
const FEEDER_TEMPLATE_FEEDER_Y_UNITS = 0
const FEEDER_TEMPLATE_UPPER_DISCONNECTOR_Y_UNITS = 3
const FEEDER_TEMPLATE_BREAKER_Y_UNITS = 6
const FEEDER_TEMPLATE_BUS_SELECTOR_Y_UNITS = 10
const FEEDER_TEMPLATE_SIDE_EARTH_X_OFFSET_UNITS = 3
const FEEDER_TEMPLATE_SIDE_EARTH_Y_UNITS = 2
const FEEDER_TEMPLATE_SIDE_EARTH_Y_STEP_UNITS = 3

export function layoutSldDocument(
  cellModel: SldCellModel,
  graph: ElectricalGraph,
  options: GenerateSldOptions = {},
): SldDocument {
  const gridSize = normalizeGridSize(options.gridSize)
  const baseDocument = createFlatSldDocumentFromGraph(graph, {
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
  const routedConnections = baseDocument.connections.map(connection => ({
    ...connection,
    route: buildConnectionRoute(connection, elementPositionsBySourceId, elementsBySourceId, gridSize),
  }))

  return {
    ...baseDocument,
    elements,
    connections: [
      ...routedConnections,
      ...buildFeederTemplateBridgeConnections(cellModel, routedConnections, elementPositionsBySourceId, gridSize),
      ...buildFeederTemplateGroundConnections(cellModel, routedConnections, elementPositionsBySourceId),
    ],
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
      if (shouldUseFeederTemplate(bayCell)) {
        for (const [sourceId, position] of buildFeederCellPositions(bayOrigin, bayCell, gridSize)) {
          positions.set(sourceId, position)
        }
        continue
      }
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

function shouldUseFeederTemplate(bayCell: SldBayCell): boolean {
  return bayCell.cellType === "feeder" && bayCell.switchgearNodeIds.length > 0
}

function buildFeederCellPositions(
  bayOrigin: SldRoutePoint,
  bayCell: SldBayCell,
  gridSize: number,
): Map<string, SldCoordinate> {
  const positions = new Map<string, SldCoordinate>()
  const switchgearNodes = bayCell.nodes.filter(node => node.role === "switchgear")
  const breakerNodes = switchgearNodes.filter(node => node.kind === "breaker")
  const disconnectorNodes = switchgearNodes.filter(node => node.kind === "disconnector")
  const groundedDisconnectors = disconnectorNodes.filter(node => node.grounded)
  const lineDisconnectors = disconnectorNodes.filter(node => !node.grounded)
  const busSelectorNodes = sortNodesBySourceXThenY(lineDisconnectors.filter(node => node.busbarConnected))
  const upperDisconnectorNodes = sortNodesBySourcePosition(lineDisconnectors.filter(node => !node.busbarConnected))
  const busbarGroundedDisconnectors = sortNodesBySourceXThenY(groundedDisconnectors.filter(node => node.busbarConnected))
  const sideGroundedDisconnectors = sortNodesBySourcePosition(groundedDisconnectors.filter(node => !node.busbarConnected))
  const feederNodes = sortNodesBySourcePosition(bayCell.nodes.filter(node => node.role === "feeder"))
  const assignedSourceIds = new Set<string>()

  feederNodes.forEach((node, index) => assignNodePosition(
    positions,
    assignedSourceIds,
    node,
    templatePoint(
      bayOrigin,
      FEEDER_TEMPLATE_CENTER_X_UNITS,
      FEEDER_TEMPLATE_FEEDER_Y_UNITS - index * 2,
      gridSize,
    ),
  ))

  resolveBusSelectorSlots(busSelectorNodes.length).forEach((slot, index) => {
    const node = busSelectorNodes[index]
    if (!node) {
      return
    }
    assignNodePosition(positions, assignedSourceIds, node, templatePoint(
      bayOrigin,
      slot,
      FEEDER_TEMPLATE_BUS_SELECTOR_Y_UNITS,
      gridSize,
    ))
  })

  upperDisconnectorNodes.forEach((node, index) => assignNodePosition(
    positions,
    assignedSourceIds,
    node,
    templatePoint(
      bayOrigin,
      FEEDER_TEMPLATE_CENTER_X_UNITS,
      FEEDER_TEMPLATE_UPPER_DISCONNECTOR_Y_UNITS + index * 2,
      gridSize,
    ),
  ))

  breakerNodes.forEach((node, index) => assignNodePosition(
    positions,
    assignedSourceIds,
    node,
    templatePoint(
      bayOrigin,
      FEEDER_TEMPLATE_CENTER_X_UNITS,
      FEEDER_TEMPLATE_BREAKER_Y_UNITS + index * 2,
      gridSize,
    ),
  ))

  sideGroundedDisconnectors.forEach((node, index) => assignNodePosition(
    positions,
    assignedSourceIds,
    node,
    templatePoint(
      bayOrigin,
      FEEDER_TEMPLATE_CENTER_X_UNITS + FEEDER_TEMPLATE_SIDE_EARTH_X_OFFSET_UNITS,
      FEEDER_TEMPLATE_SIDE_EARTH_Y_UNITS + index * FEEDER_TEMPLATE_SIDE_EARTH_Y_STEP_UNITS,
      gridSize,
    ),
  ))

  const busSelectorPositions = resolveBusSelectorSlots(busSelectorNodes.length)
  busbarGroundedDisconnectors.forEach((node, index) => {
    const slot = busSelectorPositions[index] ?? busSelectorPositions[busSelectorPositions.length - 1]
    if (!node) {
      return
    }
    assignNodePosition(
      positions,
      assignedSourceIds,
      node,
      templatePoint(
        bayOrigin,
        (slot ?? FEEDER_TEMPLATE_CENTER_X_UNITS) + FEEDER_TEMPLATE_SIDE_EARTH_X_OFFSET_UNITS,
        FEEDER_TEMPLATE_BUS_SELECTOR_Y_UNITS,
        gridSize,
      ),
    )
  })

  bayCell.nodes
    .filter(node => !assignedSourceIds.has(node.sourceId))
    .forEach((node, index) => assignNodePosition(
      positions,
      assignedSourceIds,
      node,
      templatePoint(
        bayOrigin,
        FEEDER_TEMPLATE_RIGHT_X_UNITS + 2,
        FEEDER_TEMPLATE_UPPER_DISCONNECTOR_Y_UNITS + index * 2,
        gridSize,
      ),
    ))

  return positions
}

function assignNodePosition(
  positions: Map<string, SldCoordinate>,
  assignedSourceIds: Set<string>,
  node: SldCellNode,
  position: SldCoordinate,
) {
  positions.set(node.sourceId, position)
  assignedSourceIds.add(node.sourceId)
}

function resolveBusSelectorSlots(count: number): number[] {
  if (count <= 0) {
    return []
  }
  if (count === 1) {
    return [FEEDER_TEMPLATE_CENTER_X_UNITS]
  }
  return [FEEDER_TEMPLATE_LEFT_X_UNITS, FEEDER_TEMPLATE_RIGHT_X_UNITS]
}

function templatePoint(
  bayOrigin: SldRoutePoint,
  xUnits: number,
  yUnits: number,
  gridSize: number,
): SldCoordinate {
  return {
    x: bayOrigin.x + toGridCoordinate(xUnits, gridSize),
    y: bayOrigin.y + toGridCoordinate(yUnits, gridSize),
  }
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

function buildFeederTemplateBridgeConnections(
  cellModel: SldCellModel,
  existingConnections: SldConnection[],
  positionsBySourceId: Map<string, SldRoutePoint>,
  gridSize: number,
): SldConnection[] {
  const connections: SldConnection[] = []

  for (const lane of cellModel.voltageLevels) {
    for (const bayCell of lane.bayCells) {
      if (!shouldUseFeederTemplate(bayCell)) {
        continue
      }

      const busSelectorNodes = bayCell.nodes
        .filter(node => node.role === "switchgear" && node.kind === "disconnector" && !node.grounded && node.busbarConnected)
        .sort(compareCellNodesByPosition(positionsBySourceId, "x-then-y"))
      if (busSelectorNodes.length === 0) {
        continue
      }

      const centralNodes = bayCell.nodes
        .filter(node => node.role === "switchgear" && !node.grounded && !node.busbarConnected)
        .sort(compareCellNodesByPosition(positionsBySourceId, "y-then-x"))
      const bridgeSource = centralNodes[centralNodes.length - 1]
      if (!bridgeSource || hasExistingConnectionBetween(existingConnections, bridgeSource.sourceId, busSelectorNodes.map(node => node.sourceId))) {
        continue
      }

      const sourcePoint = positionsBySourceId.get(bridgeSource.sourceId)
      const busSelectorPoints = busSelectorNodes
        .map(node => positionsBySourceId.get(node.sourceId))
        .filter((point): point is SldRoutePoint => point !== undefined)
      if (!sourcePoint || busSelectorPoints.length === 0) {
        continue
      }

      const busSelectorY = snapNumber(average(busSelectorPoints.map(point => point.y)), gridSize)
      const points = uniqueConsecutivePoints([
        sourcePoint,
        { x: sourcePoint.x, y: busSelectorY },
      ])
      if (points.length < 2) {
        continue
      }

      connections.push({
        id: `connection:feeder-template-bridge:${sanitizeId(bayCell.id)}`,
        kind: "connectivity-node",
        junctionId: `junction:feeder-template-bridge:${sanitizeId(bayCell.id)}`,
        sourceConnectivityNode: `feeder-template-bridge:${bayCell.name}`,
        portIds: [],
        terminalOwnerIds: [bridgeSource.sourceId, ...busSelectorNodes.map(node => node.sourceId)],
        route: {
          kind: "orthogonal-star",
          anchor: points[points.length - 1]!,
          segments: [{
            terminalOwnerId: bridgeSource.sourceId,
            points,
          }],
        },
      })
    }
  }

  return connections
}

function buildFeederTemplateGroundConnections(
  cellModel: SldCellModel,
  existingConnections: SldConnection[],
  positionsBySourceId: Map<string, SldRoutePoint>,
): SldConnection[] {
  const connections: SldConnection[] = []

  for (const lane of cellModel.voltageLevels) {
    for (const bayCell of lane.bayCells) {
      if (!shouldUseFeederTemplate(bayCell)) {
        continue
      }

      const busSelectorNodes = bayCell.nodes
        .filter(node => node.role === "switchgear" && node.kind === "disconnector" && !node.grounded && node.busbarConnected)
        .sort(compareCellNodesByPosition(positionsBySourceId, "x-then-y"))
      const centralNodes = bayCell.nodes
        .filter(node => node.role === "switchgear" && !node.grounded && !node.busbarConnected)
        .sort(compareCellNodesByPosition(positionsBySourceId, "y-then-x"))
      const centerX = resolveCellCenterX(bayCell, positionsBySourceId)

      for (const groundNode of bayCell.nodes.filter(node => node.kind === "disconnector" && node.grounded)) {
        if (hasRoutedSegment(existingConnections, groundNode.sourceId)) {
          continue
        }

        const groundPoint = positionsBySourceId.get(groundNode.sourceId)
        if (!groundPoint) {
          continue
        }

        const targetPoint = groundNode.busbarConnected
          ? resolveNearestPointOnSameSide(groundPoint, busSelectorNodes, positionsBySourceId)
          : resolveSideGroundConnectionPoint(groundPoint, centerX, centralNodes, positionsBySourceId)
        if (!targetPoint) {
          continue
        }

        const points = uniqueConsecutivePoints([targetPoint, groundPoint])
        if (points.length < 2) {
          continue
        }

        connections.push({
          id: `connection:feeder-template-ground:${sanitizeId(groundNode.sourceId)}`,
          kind: "connectivity-node",
          junctionId: `junction:feeder-template-ground:${sanitizeId(groundNode.sourceId)}`,
          sourceConnectivityNode: `feeder-template-ground:${groundNode.label}`,
          portIds: [],
          terminalOwnerIds: [groundNode.sourceId],
          route: {
            kind: "orthogonal-star",
            anchor: targetPoint,
            segments: [{
              terminalOwnerId: groundNode.sourceId,
              points,
            }],
          },
        })
      }
    }
  }

  return connections
}

function resolveCellCenterX(
  bayCell: SldBayCell,
  positionsBySourceId: Map<string, SldRoutePoint>,
): number | null {
  const centerNode = bayCell.nodes
    .filter(node => !node.grounded && !node.busbarConnected && (node.role === "switchgear" || node.role === "feeder"))
    .map(node => positionsBySourceId.get(node.sourceId))
    .find((point): point is SldRoutePoint => point !== undefined)
  if (centerNode) {
    return centerNode.x
  }

  const anyNode = bayCell.nodes
    .map(node => positionsBySourceId.get(node.sourceId))
    .find((point): point is SldRoutePoint => point !== undefined)
  return anyNode?.x ?? null
}

function resolveNearestPointOnSameSide(
  point: SldRoutePoint,
  nodes: SldCellNode[],
  positionsBySourceId: Map<string, SldRoutePoint>,
): SldRoutePoint | null {
  const nearest = nodes
    .map(node => positionsBySourceId.get(node.sourceId))
    .filter((candidate): candidate is SldRoutePoint => candidate !== undefined)
    .sort((left, right) => Math.abs(left.x - point.x) - Math.abs(right.x - point.x))[0]
  return nearest ? { x: nearest.x, y: point.y } : null
}

function resolveSideGroundConnectionPoint(
  groundPoint: SldRoutePoint,
  centerX: number | null,
  centralNodes: SldCellNode[],
  positionsBySourceId: Map<string, SldRoutePoint>,
): SldRoutePoint | null {
  if (centerX !== null) {
    return { x: centerX, y: groundPoint.y }
  }

  return centralNodes
    .map(node => positionsBySourceId.get(node.sourceId))
    .find((point): point is SldRoutePoint => point !== undefined) ?? null
}

function hasExistingConnectionBetween(
  connections: SldConnection[],
  sourceId: string,
  targetIds: string[],
): boolean {
  const targets = new Set(targetIds)
  return connections.some(connection => (
    connection.terminalOwnerIds.includes(sourceId)
    && connection.terminalOwnerIds.some(targetId => targets.has(targetId))
  ))
}

function hasRoutedSegment(connections: SldConnection[], sourceId: string): boolean {
  return connections.some(connection => connection.route?.segments.some(segment => (
    segment.terminalOwnerId === sourceId && segment.points.length > 1
  )))
}

function compareCellNodesByPosition(
  positionsBySourceId: Map<string, SldRoutePoint>,
  mode: "x-then-y" | "y-then-x",
) {
  return (left: SldCellNode, right: SldCellNode): number => {
    const leftPoint = positionsBySourceId.get(left.sourceId)
    const rightPoint = positionsBySourceId.get(right.sourceId)
    const leftX = leftPoint?.x ?? coordinateSortValue(left.position.x)
    const rightX = rightPoint?.x ?? coordinateSortValue(right.position.x)
    const leftY = leftPoint?.y ?? coordinateSortValue(left.position.y)
    const rightY = rightPoint?.y ?? coordinateSortValue(right.position.y)

    return mode === "x-then-y"
      ? leftX - rightX || leftY - rightY || left.orderIndex - right.orderIndex
      : leftY - rightY || leftX - rightX || left.orderIndex - right.orderIndex
  }
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
    const nonBusbarEndpoints = endpointPositions.filter(item => (
      item.terminalOwnerId !== busbarEndpoint.terminalOwnerId
      && !isGroundedDisconnectorElement(item.element)
    ))
    if (nonBusbarEndpoints.length > 0) {
      return {
        kind: "orthogonal-star",
        anchor: busbarEndpoint.point,
        segments: nonBusbarEndpoints.map(({ terminalOwnerId, point, element }) => ({
          terminalOwnerId,
          points: buildEndpointRoutePoints(element, point, [
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
    segments: endpointPositions.map(({ terminalOwnerId, point, element }) => ({
      terminalOwnerId,
      points: buildEndpointRoutePoints(element, point, [
        point,
        { x: anchor.x, y: point.y },
        anchor,
      ]),
    })),
  }
}

function isGroundedDisconnectorElement(element: SldElement | null): boolean {
  return element?.kind === "disconnector" && element.grounded
}

function buildEndpointRoutePoints(
  element: SldElement | null,
  point: SldRoutePoint,
  pointsToAnchor: SldRoutePoint[],
): SldRoutePoint[] {
  const points = uniqueConsecutivePoints(pointsToAnchor)
  return element?.kind === "feeder" && points.length > 1
    ? [...points].reverse()
    : points
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

function sortNodesBySourcePosition(nodes: SldCellNode[]): SldCellNode[] {
  return [...nodes].sort((left, right) => (
    coordinateSortValue(left.position.y) - coordinateSortValue(right.position.y)
    || coordinateSortValue(left.position.x) - coordinateSortValue(right.position.x)
    || left.orderIndex - right.orderIndex
    || left.label.localeCompare(right.label, undefined, { numeric: true })
    || left.id.localeCompare(right.id)
  ))
}

function sortNodesBySourceXThenY(nodes: SldCellNode[]): SldCellNode[] {
  return [...nodes].sort((left, right) => (
    coordinateSortValue(left.position.x) - coordinateSortValue(right.position.x)
    || coordinateSortValue(left.position.y) - coordinateSortValue(right.position.y)
    || left.orderIndex - right.orderIndex
    || left.label.localeCompare(right.label, undefined, { numeric: true })
    || left.id.localeCompare(right.id)
  ))
}

function coordinateSortValue(value: number | null): number {
  return Number.isFinite(value) ? (value as number) : 0
}

function sanitizeId(value: string): string {
  return value.trim().replace(/[\s/]+/g, "_") || "unnamed"
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
