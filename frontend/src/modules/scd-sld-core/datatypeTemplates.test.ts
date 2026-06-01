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
      <DO name="Cnt" type="MV_DO" desc="Counted measured value"/>
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
              <FCD ldInst="LD1" lnClass="LLN0" doName="A" daName="mag" fc="${mvFc}"/>
              <FCDA ldInst="LD1" lnClass="LLN0" doName="A" daName="mag.f" fc="${mvFc}"/>
              <FCD ldInst="LD1" lnClass="LLN0" doName="Cnt" daName="mag" fc="${mvFc}"/>
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

  it("resolves structured DA members and direct leaf members", () => {
    const subscription = getSubscription()
    const structured = getEntry(subscription, "A.mag[MX]")
    const direct = getEntry(subscription, "A.mag.f[MX]")
    expect(structured.leaves.map(leaf => leaf.daPath.join(".")).sort()).toEqual(["mag.f"])
    expect(direct.leaves.map(leaf => leaf.daPath.join(".")).sort()).toEqual(["mag.f"])
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

  it("resolves dotted DO references as DO plus internal path", () => {
    const fixture = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <DataTypeTemplates>
    <LNodeType id="LT1" lnClass="MMXU">
      <DO name="PhV" type="PhV_DO"/>
    </LNodeType>
    <DOType id="PhV_DO" cdc="CMV">
      <SDO name="phsC" type="Phase_DO"/>
    </DOType>
    <DOType id="Phase_DO" cdc="CMV">
      <DA name="cVal" bType="Struct" type="CValType" fc="MX"/>
    </DOType>
    <DAType id="CValType">
      <BDA name="mag" bType="Struct" type="Magnitude" fc="MX"/>
    </DAType>
    <DAType id="Magnitude">
      <BDA name="f" bType="FLOAT32" fc="MX"/>
    </DAType>
  </DataTypeTemplates>
  <IED name="IED1" type="TestIED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD1">
          <LN lnClass="MMXU" inst="1" lnType="LT1">
            <DataSet name="AllSignals">
              <FCDA ldInst="LD1" lnClass="MMXU" lnInst="1" doName="PhV.phsC" fc="MX"/>
            </DataSet>
            <ReportControl name="BRCB1" datSet="AllSignals" rptID="rpt1" buffered="true" confRev="1"/>
          </LN>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`
    const subscription = getSubscription(buildModel(fixture))
    const entry = getEntry(subscription, "PhV.phsC[MX]")
    expect(entry.leaves).toHaveLength(1)
    expect(entry.leaves[0]?.doName).toBe("PhV")
    expect(entry.leaves[0]?.reference).toBe("LD1/MMXU1.PhV.phsC.cVal.mag.f[MX]")
    expect(entry.leaves[0]?.daPath.join(".")).toBe("phsC.cVal.mag.f")
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.missing-do")).toBe(false)
  })

  it("resolves multiple dotted DO references as DO plus internal paths", () => {
    const fixture = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <DataTypeTemplates>
    <LNodeType id="LT1" lnClass="MMXU">
      <DO name="PhV" type="PHASE_DO"/>
      <DO name="A" type="PHASE_DO"/>
      <DO name="PPV" type="PAIR_DO"/>
    </LNodeType>
    <DOType id="PHASE_DO" cdc="CMV">
      <SDO name="phsA" type="PHASE_VALUE_DO"/>
      <SDO name="phsB" type="PHASE_VALUE_DO"/>
      <SDO name="phsC" type="PHASE_VALUE_DO"/>
    </DOType>
    <DOType id="PAIR_DO" cdc="CMV">
      <SDO name="phsAB" type="PHASE_VALUE_DO"/>
      <SDO name="phsBC" type="PHASE_VALUE_DO"/>
      <SDO name="phsCA" type="PHASE_VALUE_DO"/>
    </DOType>
    <DOType id="PHASE_VALUE_DO" cdc="MV">
      <DA name="cVal" bType="Struct" type="CValType" fc="MX"/>
    </DOType>
    <DAType id="CValType">
      <BDA name="mag" bType="Struct" type="Magnitude" fc="MX"/>
    </DAType>
    <DAType id="Magnitude">
      <BDA name="f" bType="FLOAT32" fc="MX"/>
    </DAType>
  </DataTypeTemplates>
  <IED name="IED1" type="TestIED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD1">
          <LN lnClass="MMXU" inst="1" lnType="LT1">
            <DataSet name="AllSignals">
              <FCDA ldInst="LD1" lnClass="MMXU" lnInst="1" doName="PhV.phsA" fc="MX"/>
              <FCDA ldInst="LD1" lnClass="MMXU" lnInst="1" doName="PhV.phsB" fc="MX"/>
              <FCDA ldInst="LD1" lnClass="MMXU" lnInst="1" doName="PhV.phsC" fc="MX"/>
              <FCDA ldInst="LD1" lnClass="MMXU" lnInst="1" doName="A.phsB" fc="MX"/>
              <FCDA ldInst="LD1" lnClass="MMXU" lnInst="1" doName="PPV.phsAB" fc="MX"/>
            </DataSet>
            <ReportControl name="BRCB1" datSet="AllSignals" rptID="rpt1" buffered="true" confRev="1"/>
          </LN>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`
    const subscription = getSubscription(buildModel(fixture))
    for (const [fragment, reference] of [
      ["PhV.phsA[MX]", "LD1/MMXU1.PhV.phsA.cVal.mag.f[MX]"],
      ["PhV.phsB[MX]", "LD1/MMXU1.PhV.phsB.cVal.mag.f[MX]"],
      ["PhV.phsC[MX]", "LD1/MMXU1.PhV.phsC.cVal.mag.f[MX]"],
      ["A.phsB[MX]", "LD1/MMXU1.A.phsB.cVal.mag.f[MX]"],
      ["PPV.phsAB[MX]", "LD1/MMXU1.PPV.phsAB.cVal.mag.f[MX]"],
    ] as const) {
      const entry = getEntry(subscription, fragment)
      expect(entry.leaves).toHaveLength(1)
      expect(entry.leaves[0]?.reference).toBe(reference)
      expect(entry.leaves[0]?.doName).toBe(fragment.split(".")[0])
      expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.missing-do")).toBe(false)
    }
  })

  it("resolves VisString and Unicode primitive string bTypes", () => {
    const fixture = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <DataTypeTemplates>
    <LNodeType id="LT1" lnClass="LLN0">
      <DO name="Note" type="NOTE_DO"/>
    </LNodeType>
    <DOType id="NOTE_DO" cdc="ENC">
      <DA name="d" bType="VisString32" fc="ST"/>
      <DA name="longText" bType="VisString64" fc="ST"/>
      <DA name="label" bType="VisString65" fc="ST"/>
      <DA name="description" bType="VisString129" fc="ST"/>
      <DA name="vendor" bType="VisString255" fc="ST"/>
      <DA name="unicode" bType="Unicode255" fc="ST"/>
    </DOType>
  </DataTypeTemplates>
  <IED name="IED1" type="TestIED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD1">
          <LN0 lnType="LT1">
            <DataSet name="AllSignals">
              <FCDA ldInst="LD1" lnClass="LLN0" doName="Note" fc="ST"/>
            </DataSet>
            <ReportControl name="BRCB1" datSet="AllSignals" rptID="rpt1" buffered="true" confRev="1"/>
          </LN0>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`
    const subscription = getSubscription(buildModel(fixture))
    const entry = getEntry(subscription, "Note[ST]")
    expect(entry.leaves.map(leaf => leaf.bType).sort()).toEqual([
      "Unicode255",
      "VisString129",
      "VisString255",
      "VisString32",
      "VisString64",
      "VisString65",
    ])
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.unknown-btype")).toBe(false)
  })

  it("inherits fc for nested BDA leaves when the child has no own fc", () => {
    const inheritedFixture = buildBaseFixture().replace('<BDA name="f" bType="FLOAT32" fc="MX"/>', '<BDA name="f" bType="FLOAT32"/>')
    const subscription = getSubscription(buildModel(inheritedFixture))
    const entry = getEntry(subscription, "A.mag[MX]")
    expect(entry.leaves).toHaveLength(1)
    expect(entry.leaves[0]?.fc).toBe("MX")
  })

  it("honors an explicit child fc on nested BDA leaves", () => {
    const overrideFixture = buildBaseFixture()
      .replace('<BDA name="f" bType="FLOAT32" fc="MX"/>', '<BDA name="f" bType="FLOAT32" fc="ST"/>')
      .replace('<FCDA ldInst="LD1" lnClass="LLN0" doName="A" daName="mag.f" fc="MX"/>', '<FCDA ldInst="LD1" lnClass="LLN0" doName="A" daName="mag.f" fc="ST"/>')
    const subscription = getSubscription(buildModel(overrideFixture))
    const entry = getEntry(subscription, "A.mag.f[ST]")
    expect(entry.leaves).toHaveLength(1)
    expect(entry.leaves[0]?.fc).toBe("ST")
  })

  it("treats Tcmd as an enum-like control leaf", () => {
    const fixture = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <DataTypeTemplates>
    <LNodeType id="LT1" lnClass="LLN0">
      <DO name="Tap" type="TAP_DO"/>
    </LNodeType>
    <DOType id="TAP_DO" cdc="ENC">
      <DA name="tapCtl" bType="Struct" type="TapCtlType" fc="ST"/>
    </DOType>
    <DAType id="TapCtlType">
      <BDA name="ctlVal" bType="Tcmd" type="Enum" fc="ST"/>
      <BDA name="origin" bType="Struct" type="OriginType" fc="ST"/>
    </DAType>
    <DAType id="OriginType">
      <BDA name="orCat" bType="Enum" type="OriginatorCategoryKind" fc="ST"/>
    </DAType>
    <EnumType id="OriginatorCategoryKind">
      <EnumVal ord="0" desc="unknown"/>
    </EnumType>
  </DataTypeTemplates>
  <IED name="IED1" type="TestIED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD1">
          <LN0 lnType="LT1">
            <DataSet name="AllSignals">
              <FCDA ldInst="LD1" lnClass="LLN0" doName="Tap" fc="ST"/>
            </DataSet>
            <ReportControl name="BRCB1" datSet="AllSignals" rptID="rpt1" buffered="true" confRev="1"/>
          </LN0>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`
    const subscription = getSubscription(buildModel(fixture))
    const entry = getEntry(subscription, "Tap[ST]")
    const tcmdLeaf = entry.leaves.find(leaf => leaf.bType === "Tcmd")
    expect(tcmdLeaf).toBeTruthy()
    expect(tcmdLeaf?.enumType).toBe("Enum")
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.unknown-btype")).toBe(false)
  })

  it("fails closed when enum type is unknown", () => {
    const model = buildModel(buildBaseFixture({ ctlEnumTypeId: "MissingEnumType" }))
    const subscription = getSubscription(model)
    const entry = getEntry(subscription, "CtlModel.ctlModel")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.unknown-enumtype")).toBe(true)
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

  it("marks incompatible fc members as unresolved when no exact leaf exists", () => {
    const model = buildModel(buildBaseFixture({ ctlFc: "CF", ctlMemberFc: "MX" }))
    const subscription = getSubscription(model)
    const entry = getEntry(subscription, "CtlModel.ctlModel")
    expect(entry.leaves).toHaveLength(0)
    const diagnostic = entry.diagnostics.find(item => item.code === "datatype-templates.incompatible-fc")
    expect(diagnostic).toBeTruthy()
    expect(diagnostic?.message).toContain("no reachable leaf with FC=MX")
    expect(diagnostic?.message).not.toContain("Selected candidate:")
  })

  it("does not pick a non-ST fallback candidate for ODDBSw[ST]", () => {
    const fixture = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <DataTypeTemplates>
    <LNodeType id="LT1" lnClass="LDBI">
      <DO name="ODDBSw" type="ODDBSw_DO"/>
    </LNodeType>
    <DOType id="ODDBSw_DO" cdc="ENC">
      <DA name="ctlModel" bType="Enum" type="CtlModelKind" fc="CF"/>
      <DA name="Oper" bType="Struct" type="OperType" fc="CO"/>
      <DA name="cdcName" bType="VisString64" fc="EX"/>
      <DA name="dataNs" bType="VisString255" fc="EX"/>
      <DA name="opRcvd" bType="BOOLEAN" fc="OR"/>
    </DOType>
    <DAType id="OperType">
      <BDA name="ctlVal" bType="Enum" type="CtlValKind" fc="CO"/>
      <BDA name="origin" bType="Struct" type="OriginType" fc="CO"/>
      <BDA name="ctlNum" bType="INT8U" fc="CO"/>
      <BDA name="T" bType="Timestamp" fc="CO"/>
    </DAType>
    <DAType id="OriginType">
      <BDA name="orCat" bType="Enum" type="OriginatorCategoryKind" fc="CO"/>
      <BDA name="orIdent" bType="VisString64" fc="CO"/>
    </DAType>
    <EnumType id="CtlModelKind">
      <EnumVal ord="0" desc="status-only"/>
      <EnumVal ord="1" desc="direct-with-normal-security"/>
    </EnumType>
    <EnumType id="CtlValKind">
      <EnumVal ord="0" desc="off"/>
      <EnumVal ord="1" desc="on"/>
    </EnumType>
    <EnumType id="OriginatorCategoryKind">
      <EnumVal ord="0" desc="unknown"/>
    </EnumType>
  </DataTypeTemplates>
  <IED name="IED1" type="TestIED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD0">
          <LN lnClass="LDBI" inst="1" lnType="LT1">
            <DataSet name="AllSignals">
              <FCDA ldInst="LD0" lnClass="LDBI" lnInst="1" doName="ODDBSw" fc="ST"/>
            </DataSet>
            <ReportControl name="BRCB1" datSet="AllSignals" rptID="rpt1" buffered="true" confRev="1"/>
          </LN>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`
    const subscription = getSubscription(buildModel(fixture))
    const entry = getEntry(subscription, "ODDBSw[ST]")
    expect(entry.leaves).toHaveLength(0)
    const diagnostic = entry.diagnostics.find(item => item.code === "datatype-templates.incompatible-fc")
    expect(diagnostic).toBeTruthy()
    expect(diagnostic?.message).toContain("no reachable leaf with FC=ST")
    expect(diagnostic?.message).toContain("CF: LD0/LDBI1.ODDBSw.ctlModel[CF]")
    expect(diagnostic?.message).toContain("CO: LD0/LDBI1.ODDBSw.Oper.ctlVal[CO]")
    expect(diagnostic?.message).toContain("EX: LD0/LDBI1.ODDBSw.cdcName[EX]")
    expect(diagnostic?.message).toContain("OR: LD0/LDBI1.ODDBSw.opRcvd[OR]")
    expect(diagnostic?.message).not.toContain("Selected candidate:")
  })

  it("fails closed when count > 1 is encountered", () => {
    const countFixture = buildBaseFixture().replace('<BDA name="f" bType="FLOAT32" fc="MX"/>', '<BDA name="f" bType="FLOAT32" fc="MX" count="2"/>')
    const subscription = getSubscription(buildModel(countFixture))
    const entry = getEntry(subscription, "A.mag[MX]")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.array-count-not-supported")).toBe(true)
  })

  it("fails closed when bType is unknown", () => {
    const model = buildModel(buildBaseFixture({ unknownBType: "Mystery" }))
    const subscription = getSubscription(model)
    const entry = getEntry(subscription, "A[MX]")
    expect(entry.leaves).toHaveLength(0)
    expect(entry.diagnostics.some(diagnostic => diagnostic.code === "datatype-templates.unknown-btype")).toBe(true)
  })
})
