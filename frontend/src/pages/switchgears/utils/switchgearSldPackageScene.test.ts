import { describe, expect, it } from "vitest"

import type { Switchgear } from "@/types/switchgear"

import { buildSwitchgearSldPackageSceneModel, SWITCHGEAR_SLD_STAGE_PADDING } from "./switchgearSldPackageScene"
import type { StoredDiagramState } from "./switchgearSldDiagramTypes"

describe("switchgearSldPackageScene", () => {
  it("maps legacy switchgear layout and port-bound edges into diagram-core scene input", () => {
    const switchgears: Switchgear[] = [
      {
        id: 7,
        workspace_ids: [1],
        switchgear_type: "switchgear",
        name: "Q01",
        bindings: [],
      },
    ]
    const storedState: StoredDiagramState = {
      layoutById: {
        "7": { x: 120, y: 240 },
      },
      labelOffsetById: {
        "7": { x: 18, y: 34 },
      },
      edges: [{
        id: "edge-1",
        x1: 0,
        y1: 0,
        x2: 640,
        y2: 360,
        kind: "line",
        startBinding: {
          ownerType: "node",
          ownerId: 7,
          portId: "right",
        },
      }],
      textElements: [{
        id: "free-text",
        text: "Bay A",
        size: "md",
        x: 640,
        y: 120,
      }],
      viewState: {
        x: -1000,
        y: -500,
        zoom: 2,
      },
    }

    const result = buildSwitchgearSldPackageSceneModel(switchgears, storedState)
    const node = result.scene.nodes?.[0]
    const edge = result.scene.edges?.[0]

    expect(node).toMatchObject({
      id: "switchgear:7",
      x: 120 + SWITCHGEAR_SLD_STAGE_PADDING,
      y: 240 + SWITCHGEAR_SLD_STAGE_PADDING,
      width: 40,
      height: 40,
    })
    expect(result.scene.ports?.map(port => port.id)).toContain("node:7:right")
    expect(edge).toMatchObject({
      id: "edge-1",
      source: {
        kind: "port",
        portId: "node:7:right",
      },
      target: {
        kind: "point",
        point: { x: 640, y: 360 },
      },
    })
    expect(result.scene.texts?.map(text => text.id)).toEqual(expect.arrayContaining([
      "switchgear-label:7",
      "free-text",
    ]))
    expect(result.scene.viewport).toMatchObject({
      x: 500,
      y: 250,
      zoom: 2,
    })
  })
})
