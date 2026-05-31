import { describe, expect, it } from "vitest"

import { parseScdSource } from "./parser"

function buildBaseFixture(overrides: {
  spcDoTypeId?: string
  dpsDoTypeId?: string
  mvDaTypeId?: string
  cmvDaTypeId?: string
  ctlEnumTypeId?: string
  stValFc?: string
  mvFc?: string
  cmvFc?: string
  ctlFc?: string
  ctlMemberFc?: string
  unknownBType?: string | null
} = {}): string {
  const spcDoTypeId = overrides.spcDoTypeId ?? "SPS_DO"
  const dpsDoTypeId = overrides.dpsDoTypeId ?? "DPS_DO"
  const mvDaTypeId = overrides.mvDaTypeId ?? "Magnitude"
  const cmvDaTypeId = overrides.cmvDaTypeId ?? "PhaseValue"
  const ctlEnumTypeId = overrides.ctlEnumTypeId ?? "CtlModelType"
  const stValFc = overrides.stValFc ?? "ST"
  const mvFc = overrides.mvFc ?? "MX"
  const cmvFc = overrides.cmvFc ?? "MX"
  const ctlFc = overrides.ctlFc ?? "CF"
  const ctlMemberFc = overrides.ctlMemberFc ?? ctlFc
  const unknownBType = overrides.unknownBType

  const mvBType = unknownBType ?? "Struct"
  const cmvBType = unknownBType ?? "Struct"

  return `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <DataTypeTemplates>
    <LNodeType id="LT1" lnClass="LLN0">
      <DO name="SPCSO1" type="${spcDoTypeId}" desc="Single point status"/>
      <DO name="DPCSO1" type="${dpsDoTypeId}" desc="Double point status"/>
      <DO name="A" type="MV_DO" desc="Measured value"/>
      <DO name="W" type="CMV_DO" desc="Complex measured value"/>
      <DO name="CtlModel" type="CTL_DO" desc="Enum control model"/>
    </LNodeType>

    <DOType id="SPS_DO" cdc="SPS">
      <DA name="stVal" bType="BOOLEAN" fc="${stValFc}"/>
      <DA name="q" bType="Quality" fc="${stValFc}"/>
      <DA name="t" bType="Timestamp" fc="${stValFc}"/>
    </DOType>

    <DOType id="DPS_DO" cdc="DPS">
      <DA name="stVal" bType="BOOLEAN" fc="${stValFc}"/>
      <DA name="q" bType="Quality" fc="${stValFc}"/>
      <DA name="t" bType="Timestamp" fc="${stValFc}"/>
    </DOType>

    <DOType id="MV_DO" cdc="MV">
      <DA name="mag" bType="${mvBType}" type="${mvDaTypeId}" fc="${mvFc}"/>
      <DA name="q" bType="Quality" fc="${mvFc}"/>
      <DA name="t" bType="Timestamp" fc="${mvFc}"/>
    </DOType>

    <DOType id="CMV_DO" cdc="CMV">
      <DA name="phsA" bType="${cmvBType}" type="${cmvDaTypeId}" fc="${cmvFc}"/>
      <DA name="q" bType="Quality" fc="${cmvFc}"/>
      <DA name="t" bType="Timestamp" fc="${cmvFc}"/>
    </DOType>

    <DOType id="CTL_DO" cdc="ENC">
      <DA name="ctlModel" bType="Enum" type="${ctlEnumTypeId}" fc="${ctlFc}"/>
    </DOType>

    <DAType id="Magnitude">
      <BDA name="f" bType="FLOAT32" fc="${mvFc}"/>
    </DAType>

    <DAType id="CValType">
      <BDA name="mag" bType="Struct" type="Magnitude" fc="${cmvFc}"/>
    </DAType>

    <DAType id="PhaseValue">
      <BDA name="cVal" bType="Struct" type="CValType" fc="${cmvFc}"/>
    </DAType>

    <EnumType id="CtlModelType">
      <EnumVal ord="0" desc="status-only"/>
      <EnumVal ord="1" desc="direct-with-normal-security"/>
    </EnumType>
  </DataTypeTemplates>

  <IED name="IED1" type="TestIED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD1">
          <LN0 lnType="LT1">
            <DataSet name="AllSignals">
              <FCDA ldInst="LD1" lnClass="LLN0" doName="SPCSO1" daName="stVal" fc="${stValFc}"/>
              <FCD ldInst="LD1" lnClass="LLN0" doName="SPCSO1" fc="${stValFc}"/>
              <FCD ldInst="LD1" lnClass="LLN0" doName="DPCSO1" fc="${stValFc}"/>
              <FCD ldInst="LD1" lnClass="LLN0" doName="A" fc="${mvFc}"/>
              <FCD ldInst="LD1" lnClass="LLN0" doName="W" fc="${cmvFc}"/>
              <FCDA ldInst="LD1" lnClass="LLN0" doName="CtlModel" daName="ctlModel" fc="${ctlMemberFc}"/>
            </DataSet>
            <ReportControl name="BRCB1" datSet="AllSignals" rptID="rpt1" buffered="true" confRev="1"/>
          </LN0>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`
}

function buildModel(xmlText: string) {
  return parseScdSource({
    fileName: "fixture.scd",
    contentHash: "hash",
    xmlText,
  })
}

function getSubscription(model = buildModel(buildBaseFixture())) {
  const subscription = model.reportSubscriptions.find(candidate => candidate.reportControlName === "BRCB1")
  expect(subscription).toBeTruthy()
  return subscription!
}

function getEntry(subscription: ReturnType<typeof getSubscription>, fragment: string) {
  const entry = subscription.normalizedDatasetEntries.find(item => item.memberRef.includes(fragment))
  expect(entry).toBeTruthy()
  return entry!
}

describe("IEC 61850 DataTypeTemplates normalization", () => {
  it("resolves a direct FCDA leaf", () => {
    const subscription = getSubscription()
    const entry = getEntry(subscription, "SPCSO1.stVal")
    expect(entry.sourceKind).toBe("FCDA")
    expect(entry.leaves).toHaveLength(1)
    expect(entry.leaves[0]?.doName).toBe("SPCSO1")
    expect(entry.leaves[0]?.daPath).toEqual(["stVal"])
    expect(entry.leaves[0]?.fc).toBe("ST")
  })

  it("expands an FCD parent DO into multiple leaves", () => {
    const subscription = getSubscription()
    const entry = getEntry(subscription, "SPCSO1[ST]")
    expect(entry.sourceKind).toBe("FCD")
    expect(entry.leaves.map(leaf => leaf.daPath.join(".")).sort()).toEqual(["q", "stVal", "t"])
  })

  it("resolves SPS-like and DPS-like reportables", () => {
    const subscription = getSubscription()
    const sps = getEntry(subscription, "SPCSO1[ST]")
    const dps = getEntry(subscription, "DPCSO1[ST]")
    expect(sps.leaves.map(leaf => leaf.daPath.join("."))).toEqual(["stVal", "q", "t"])
    expect(dps.leaves.map(leaf => leaf.daPath.join("."))).toEqual(["stVal", "q", "t"])
  })

  it("resolves MV-like leaves", () => {
    const subscription = getSubscription()
    const entry = getEntry(subscription, "A[MX]")
    expect(entry.leaves.map(leaf => leaf.daPath.join(".")).sort()).toEqual(["mag.f", "q", "t"])
  })

  it("resolves CMV-like nested leaves", () => {
    const subscription = getSubscription()
    const entry = getEntry(subscription, "W[MX]")
    expect(entry.leaves.map(leaf => leaf.daPath.join(".")).sort()).toEqual(["phsA.cVal.mag.f", "q", "t"])
  })

  it("resolves enum DA through EnumType", () => {
    const subscription = getSubscription()
    const entry = getEntry(subscription, "CtlModel.ctlModel")
    expect(entry.leaves).toHaveLength(1)
    expect(entry.leaves[0]?.enumType).toBe("CtlModelType")
    expect(entry.leaves[0]?.bType).toBe("Enum")
  })

  it("uses the same resolver path for FCDA and FCD members", () => {
    const subscription = getSubscription()
    const direct = getEntry(subscription, "SPCSO1.stVal")
    const parent = getEntry(subscription, "SPCSO1[ST]")
    expect(direct.leaves[0]?.reference).toBe(parent.leaves[0]?.reference)
    expect(direct.leaves[0]?.daPath).toEqual(parent.leaves[0]?.daPath)
  })

  it("fails closed when DOType is missing", () => {
    const model = buildModel(buildBaseFixture({ spcDoTypeId: "MissingDoType" }))
    const subscription = model.reportSubscriptions.find(candidate => candidate.reportControlName === "BRCB1")
    expect(subscription).toBeTruthy()
    const entry = getEntry(subscription!, "SPCSO1[ST]")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.missing-dotype")).toBe(true)
  })

  it("fails closed when DAType is missing", () => {
    const model = buildModel(buildBaseFixture({ mvDaTypeId: "MissingDaType" }))
    const subscription = getSubscription(model)
    const entry = getEntry(subscription, "A[MX]")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.missing-datype")).toBe(true)
  })

  it("fails closed when fc is incompatible", () => {
    const model = buildModel(buildBaseFixture({ ctlFc: "CF", ctlMemberFc: "MX" }))
    const subscription = getSubscription(model)
    const entry = getEntry(subscription, "CtlModel.ctlModel")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.incompatible-fc")).toBe(true)
  })

  it("fails closed when bType is unknown", () => {
    const model = buildModel(buildBaseFixture({ unknownBType: "Mystery" }))
    const subscription = getSubscription(model)
    const entry = getEntry(subscription, "A[MX]")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.unknown-btype")).toBe(true)
  })
})
