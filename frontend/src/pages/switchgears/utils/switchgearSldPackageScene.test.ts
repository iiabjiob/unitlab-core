import { describe, expect, it } from "vitest"

import type { Switchgear } from "@/types/switchgear"

import { buildSwitchgearSldPackageSceneModel, serializeSwitchgearSldPackageScene, SWITCHGEAR_SLD_STAGE_PADDING } from "./switchgearSldPackageScene"
import type { StoredDiagramState } from "./switchgearSldDiagramTypes"

describe("switchgearSldPackageScene", () => {
  it("maps legacy switchgear layout and port-bound edges into diagram-core scene input", () => {
    const switchgears: Switchgear[] = [{
      id: 7,
      workspace_ids: [1],
      switchgear_type: "switchgear",
      name: "Q01",
      bindings: [],
    }]
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
      metadata: {
        labelOffsetX: 18,
        labelOffsetY: 34,
      },
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
    expect(result.scene.texts?.map(text => text.id)).toEqual(["free-text"])
    expect(result.scene.viewport).toMatchObject({
      x: 500,
      y: 250,
      zoom: 2,
    })
  })

  it("serializes package scene edits back into legacy stored state", () => {
    const model = buildSwitchgearSldPackageSceneModel([{ id: 7, workspace_ids: [1], switchgear_type: "switchgear", name: "Q01", bindings: [] }], {
      workspaceId: 1,
      layoutById: { "7": { x: 120, y: 240 } },
      labelOffsetById: { "7": { x: 18, y: 34 } },
      edges: [{
        id: "edge-1",
        x1: 160,
        y1: 260,
        x2: 640,
        y2: 360,
        kind: "line",
        startBinding: { ownerType: "node", ownerId: 7, portId: "right" },
      }],
      textElements: [{ id: "free-text", text: "Bay A", size: "md", x: 640, y: 120 }],
      viewState: { x: -1000, y: -500, zoom: 2 },
      snapEnabled: true,
    })

    const serialized = serializeSwitchgearSldPackageScene({
      ...model.scene,
      nodes: model.scene.nodes?.map(node => node.id === "switchgear:7" ? { ...node, x: node.x + 48 } : node) ?? [],
      texts: model.scene.texts?.map(text => text.id === "free-text" ? { ...text, text: "Bay B", x: 700 } : text) ?? [],
      viewport: { x: 520, y: 260, width: 900, height: 600, zoom: 2 },
      selection: { ids: [], primaryId: null },
      ports: model.scene.ports ?? [],
      edges: model.scene.edges ?? [],
      shapes: model.scene.shapes ?? [],
    }, {
      workspaceId: 1,
      snapEnabled: true,
      labelOffsetById: { "7": { x: 18, y: 34 } },
      baseState: {
        workspaceId: 1,
        labelOffsetById: { "7": { x: 18, y: 34 } },
        snapEnabled: true,
      },
    })

    expect(serialized.layoutById).toEqual({
      "7": { x: 168, y: 240 },
    })
    expect(serialized.textElements).toEqual([{
      id: "free-text",
      text: "Bay B",
      size: "md",
      x: 700,
      y: 120,
    }])
    expect(serialized.edges?.[0]).toMatchObject({
      startBinding: { ownerType: "node", ownerId: 7, portId: "right" },
      kind: "line",
    })
    expect(serialized.viewState).toEqual({ x: -1040, y: -520, zoom: 2 })
  })

  it("keeps a binding when its port is temporarily missing", () => {
    const model = buildSwitchgearSldPackageSceneModel([], {
      edges: [{
        id: "edge-missing-port",
        x1: 120,
        y1: 160,
        x2: 240,
        y2: 160,
        kind: "line",
        startBinding: { ownerType: "node", ownerId: 7, portId: "right" },
      }],
    })

    const serialized = serializeSwitchgearSldPackageScene({
      ...model.scene,
      ports: [],
      edges: model.scene.edges ?? [],
      nodes: model.scene.nodes ?? [],
      shapes: model.scene.shapes ?? [],
      texts: model.scene.texts ?? [],
      viewport: { x: 0, y: 0, width: 800, height: 600, zoom: 1 },
      selection: { ids: [], primaryId: null },
    })

    expect(serialized.edges?.[0]?.startBinding).toEqual({ ownerType: "node", ownerId: 7, portId: "right" })
  })
})
