import type { HeaderRenderableEntry } from "../../../types/internal.js"
import type { UiTableBodyBindings } from "../../bindings.js"
import type { ColumnBlueprint, ImperativeBodyRegion } from "../../types.js"

/**
 * Maintains column blueprints derived from header metrics.
 */
export class BlueprintManager {
  private blueprints: ColumnBlueprint[] = []
  private blueprintMap = new Map<string, ColumnBlueprint>()
  private columnIndices: number[] = []
  private blueprintsByRegion = new Map<ImperativeBodyRegion, ColumnBlueprint[]>()

  constructor(private readonly body: UiTableBodyBindings) {}

  rebuild(entriesByRegion: Partial<Record<ImperativeBodyRegion, HeaderRenderableEntry[]>>, order: ImperativeBodyRegion[]) {
    const result: ColumnBlueprint[] = []
    const lookup = new Map<string, ColumnBlueprint>()
    const indices: number[] = []

    const regionMap = new Map<ImperativeBodyRegion, ColumnBlueprint[]>()

    for (const region of order) {
      const regionEntries = entriesByRegion[region] ?? []
      if (!regionEntries.length) {
        regionMap.set(region, [])
        continue
      }
      const regionBlueprints: ColumnBlueprint[] = []
      for (const entry of regionEntries) {
        const column = entry.metric.column
        const binding = this.body.getColumnBinding(column.key)
        const blueprint: ColumnBlueprint = {
          renderable: entry,
          column,
          binding,
          cachedClassName: "",
          region,
        }
        result.push(blueprint)
        regionBlueprints.push(blueprint)
        lookup.set(column.key, blueprint)
        indices.push(binding.columnIndex)
      }
      regionMap.set(region, regionBlueprints)
    }

    this.blueprints = result
    this.blueprintMap = lookup
    this.columnIndices = indices
    this.blueprintsByRegion = regionMap
  }

  clear() {
    this.blueprints = []
    this.blueprintMap = new Map()
    this.columnIndices = []
    this.blueprintsByRegion = new Map()
  }

  getBlueprints(): ColumnBlueprint[] {
    return this.blueprints
  }

  getBlueprintByKey(key: string): ColumnBlueprint | null {
    return this.blueprintMap.get(key) ?? null
  }

  getBlueprintMap(): Map<string, ColumnBlueprint> {
    return this.blueprintMap
  }

  getColumnIndices(): number[] {
    return this.columnIndices
  }

  getBlueprintsForRegion(region: ImperativeBodyRegion): ColumnBlueprint[] {
    return this.blueprintsByRegion.get(region) ?? []
  }
}
