import type {
  GenerateSldOptions,
  NormalizedSclModel,
  SclEquipment,
  SldConnection,
  SldDocument,
  SldElement,
} from "./types"

export function createSldDocument(model: NormalizedSclModel, options: GenerateSldOptions = {}): SldDocument {
  const equipment = collectEquipment(model)
  const elements = equipment.map(mapEquipmentToElement)

  return {
    schema: "unitlab.scd-sld.document",
    version: 1,
    sourceHash: model.source.contentHash,
    generatedAt: options.generatedAt ?? null,
    elements,
    connections: buildConnectivityNodeConnections(equipment),
    labels: elements.map(element => ({
      id: `${element.id}/label`,
      sourceId: element.sourceId,
      text: element.label,
      position: element.position,
    })),
    diagnostics: [...model.diagnostics],
    layoutHints: {
      generatedFrom: "scd",
      gridSize: options.gridSize ?? 24,
    },
  }
}

function collectEquipment(model: NormalizedSclModel): SclEquipment[] {
  return model.substations.flatMap(substation => [
    ...substation.powerTransformers,
    ...substation.voltageLevels.flatMap(voltageLevel => (
      voltageLevel.bays.flatMap(bay => bay.equipments)
    )),
  ])
}

function mapEquipmentToElement(equipment: SclEquipment): SldElement {
  return {
    id: `sld-element:${equipment.id}`,
    sourceId: equipment.id,
    sourcePath: equipment.sourcePath,
    kind: equipment.kind,
    label: equipment.name,
    equipmentType: equipment.type,
    substationName: equipment.substationName,
    voltageLevelName: equipment.voltageLevelName,
    bayName: equipment.bayName,
    position: equipment.coordinates,
  }
}

function buildConnectivityNodeConnections(equipment: SclEquipment[]): SldConnection[] {
  const ownersByConnectivityNode = new Map<string, Set<string>>()

  for (const item of equipment) {
    for (const terminal of item.terminals) {
      if (!terminal.connectivityNode) {
        continue
      }
      const owners = ownersByConnectivityNode.get(terminal.connectivityNode) ?? new Set<string>()
      owners.add(item.id)
      ownersByConnectivityNode.set(terminal.connectivityNode, owners)
    }
  }

  return Array.from(ownersByConnectivityNode.entries())
    .filter(([, owners]) => owners.size > 1)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([connectivityNode, owners]) => ({
      id: `connection:${sanitizeId(connectivityNode)}`,
      kind: "connectivity-node",
      sourceConnectivityNode: connectivityNode,
      terminalOwnerIds: Array.from(owners).sort((left, right) => left.localeCompare(right)),
    }))
}

function sanitizeId(value: string): string {
  return value.trim().replace(/[\s/]+/g, "_")
}
