import { createMeasurementQueue } from "@/ui-table/core/runtime/measurementQueue"
import type { ImperativeControllerSuite, ImperativeControllerFactoryContext } from "@/ui-table/core/imperative/engine/ImperativeTableEngine"
import { BlueprintManager } from "@/ui-table/core/imperative/engine/controllers/BlueprintManager"
import { CellRenderer } from "@/ui-table/core/imperative/engine/controllers/CellRenderer"
import { RowController } from "@/ui-table/core/imperative/engine/controllers/RowController"
import { InteractionController } from "@/ui-table/core/imperative/engine/controllers/InteractionController"
import { EditorController } from "@/ui-table/core/imperative/engine/controllers/EditorController"

export function createDomControllerSuite(context: ImperativeControllerFactoryContext): ImperativeControllerSuite {
  const measurementQueue = createMeasurementQueue()

  const blueprintManager = new BlueprintManager(context.body)
  const editor = new EditorController()
  const cellRenderer = new CellRenderer(context.body, context.classMap, context.renderer, editor)
  const rowController = new RowController(
    context.body,
    context.classMap,
    cellRenderer,
    blueprintManager,
    measurementQueue,
  )
  const interaction = new InteractionController(
    context.mode,
    context.body,
    rowController,
    blueprintManager,
    context.classMap,
    context.beginCellEdit,
    measurementQueue,
  )

  return {
    blueprintManager,
    editor,
    cellRenderer,
    rowController,
    interaction,
    dispose: () => {
      measurementQueue.dispose()
    },
  }
}
