import type {
  ImperativeControllerSuite,
  ImperativeControllerFactoryContext,
} from "./engine/ImperativeTableEngine"

function noop() {
  // no-op
}

export function createNoopControllerSuite(): ImperativeControllerSuite {
  return {
    blueprintManager: {
      rebuild: noop,
      clear: noop,
      getBlueprints: () => [],
      getBlueprintByKey: () => null,
      getBlueprintsForRegion: () => [],
      getColumnIndices: () => [],
    },
    editor: {
      open: noop,
      close: noop,
    },
    cellRenderer: {
      refreshColumnFillersMode: noop,
    },
    rowController: {
      setContainers: noop,
      getPrimaryRegion: () => "main",
      getContainer: () => null,
      findRowSlotByDisplayIndex: () => null,
      findCellSlot: () => null,
      getRowSlots: () => [],
      updateRows: () => 0,
      resetCellClassCache: noop,
      dispose: noop,
    },
    interaction: {
      detach: noop,
      dispose: noop,
      setContainers: noop,
      setMetrics: noop,
      reappendOverlay: noop,
    },
  }
}

export function createNoopControllerFactory() {
  return (_context: ImperativeControllerFactoryContext): ImperativeControllerSuite => createNoopControllerSuite()
}
