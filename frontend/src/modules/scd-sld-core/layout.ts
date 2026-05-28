import { createSldDocumentFromGraph } from "./sldDocument"
import type {
  ElectricalGraph,
  GenerateSldOptions,
  SldCellModel,
  SldCoordinate,
  SldDocument,
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
    }
  })

  return {
    ...baseDocument,
    elements,
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

function snapCoordinate(position: SldCoordinate, gridSize: number): SldCoordinate {
  return {
    x: position.x === null ? null : Math.round(position.x / gridSize) * gridSize,
    y: position.y === null ? null : Math.round(position.y / gridSize) * gridSize,
  }
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
