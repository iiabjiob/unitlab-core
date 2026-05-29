import { describe, expect, it } from "vitest"

import { parseScdSource } from "@/modules/scd-sld-core"
import { buildIec61850DebugDocument } from "./iec61850DebugTree"
import {
  buildIec61850DebugSimulatorPlan,
  runIec61850DebugSimulator,
} from "./iec61850DebugSimulator"
import type { Iec61850SignalListMergeResult } from "./iec61850SignalListMerge"

const reportScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" revision="B" version="2007">
  <IED name="IED1">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD0">
          <LN0 lnClass="LLN0" inst="" lnType="LLN0_TYPE">
            <DataSet name="dsEvents">
              <FCDA ldInst="LD0" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST"/>
              <FCDA ldInst="LD0" lnClass="GGIO" prefix="P" lnInst="1" doName="Ind1" fc="ST"/>
            </DataSet>
            <ReportControl name="brcbEvents" rptID="IED1LD0/LLN0.BR.Events" datSet="dsEvents" confRev="7" buffered="true" indexed="true" bufTime="100" intgPd="1000">
              <TrgOps dchg="true" qchg="true" dupd="false" period="false" gi="true"/>
              <OptFields seqNum="true" timeStamp="true" reasonCode="true" dataSet="true" dataRef="true" entryID="true" configRef="true" bufOvfl="true"/>
            </ReportControl>
          </LN0>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`

describe("iec61850DebugSimulator", () => {
  it("builds a report plan from merge matches and runs simulator GI explicitly", async () => {
    const document = buildIec61850DebugDocument(parseScdSource({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: reportScd,
    }))
    const mergeResult = mergeFixture()

    const plan = buildIec61850DebugSimulatorPlan(document, mergeResult)
    expect(plan.requiredReportCount).toBe(1)
    expect(plan.matchedSignalCount).toBe(2)

    const run = await runIec61850DebugSimulator(
      document,
      mergeResult,
      () => new Date("2026-05-29T12:00:00.000Z"),
    )

    expect(run.plan.requiredReportCount).toBe(1)
    expect(run.reports).toHaveLength(1)
    expect(run.reports[0]).toMatchObject({
      iedName: "IED1",
      accessPointName: "AP1",
      reportControlName: "brcbEvents",
      matchedSignalCount: 2,
      lifecycleState: "disabled",
      errorCode: null,
    })
    expect(run.reports[0]?.event?.values.map(value => value.reference)).toEqual([
      "LD0/XCBR1.Pos.stVal[ST]",
      "LD0/PGGIO1.Ind1[ST]",
    ])
    expect(run.eventLog.map(event => event.kind)).toEqual([
      "connect",
      "read",
      "disconnect",
      "connect",
      "reserve",
      "disconnect",
      "connect",
      "enable",
      "disconnect",
      "connect",
      "general-interrogation",
      "report",
      "disconnect",
      "connect",
      "disable",
      "disconnect",
      "connect",
      "release",
      "disconnect",
    ])
  })
})

function mergeFixture(): Iec61850SignalListMergeResult {
  return {
    rowCount: 2,
    addressColumn: "Full 61850 Path",
    columnConfidence: "high",
    addressRows: 2,
    matchedRows: 2,
    unmatchedRows: 0,
    rowsWithoutAddress: 0,
    matchedReports: [],
    matchedIeds: ["IED1"],
    matches: [
      {
        signalId: 1,
        signalKey: "sig-1",
        signalName: "Breaker position",
        address: "IED1LD0/XCBR1/Pos/stVal[ST]",
        modelReference: "LD0/XCBR1.Pos.stVal[ST]",
        dataSets: ["IED1/AP1/LD0/LLN0.dsEvents"],
        reports: [],
        ieds: ["IED1"],
      },
      {
        signalId: 2,
        signalKey: "sig-2",
        signalName: "Indication",
        address: "IED1LD0/PGGIO1/Ind1[ST]",
        modelReference: "LD0/PGGIO1.Ind1[ST]",
        dataSets: ["IED1/AP1/LD0/LLN0.dsEvents"],
        reports: [],
        ieds: ["IED1"],
      },
    ],
    misses: [],
  }
}
