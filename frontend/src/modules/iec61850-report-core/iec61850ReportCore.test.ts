import { describe, expect, it } from "vitest"

import { parseScdSource } from "../scd-sld-core"
import {
  createIec61850SimulatorAdapter,
  Iec61850ReportManager,
  reportControlKey,
  toReportControlRef,
  type Iec61850DeviceEndpoint,
} from "./index"

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

const endpoint: Iec61850DeviceEndpoint = {
  id: "sim:IED1/AP1",
  mode: "simulator",
  iedName: "IED1",
  accessPointName: "AP1",
  host: null,
  port: 102,
}

describe("iec61850-report-core", () => {
  it("reads simulator report control state and compares it with the SCD subscription candidate", async () => {
    const candidate = firstReportCandidate()
    const manager = new Iec61850ReportManager(createIec61850SimulatorAdapter({
      devices: [{ endpoint, reports: [candidate] }],
    }))

    const result = await manager.readReportControl(endpoint, candidate)

    expect(result.diagnostics).toEqual([])
    expect(result.state).toMatchObject({
      dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents",
      confRev: "7",
      enabled: false,
      reservedBy: null,
      triggerOptions: expect.objectContaining({ generalInterrogation: true }),
      optionalFields: expect.objectContaining({ sequenceNumber: true, configRevision: true }),
    })
  })

  it("surfaces live/SCD mismatches before any report activation", async () => {
    const candidate = firstReportCandidate()
    const reference = toReportControlRef(candidate)
    const manager = new Iec61850ReportManager(createIec61850SimulatorAdapter({
      devices: [{
        endpoint,
        reports: [candidate],
        overrides: {
          [reportControlKey(reference)]: {
            confRev: "8",
            signalCount: 1,
            triggerOptions: { ...candidate.triggerOptions, generalInterrogation: false },
          },
        },
      }],
    }))

    const result = await manager.readReportControl(endpoint, candidate)

    expect(result.diagnostics.map(diagnostic => diagnostic.code)).toEqual([
      "CONFREV_MISMATCH",
      "SIGNAL_COUNT_MISMATCH",
      "TRGOPS_MISMATCH",
    ])
  })

  it("runs simulator reservation, enable, GI, disable, and release without real devices", async () => {
    const candidate = firstReportCandidate()
    const manager = new Iec61850ReportManager(createIec61850SimulatorAdapter({
      devices: [{ endpoint, reports: [candidate] }],
      now: () => new Date("2026-05-29T12:00:00.000Z"),
    }))

    await expect(manager.reserveReportControl(endpoint, candidate, "unitlab")).resolves.toMatchObject({
      reservedBy: "unitlab",
      enabled: false,
    })
    await expect(manager.enableReportControl(endpoint, candidate, "unitlab")).resolves.toMatchObject({
      owner: "unitlab",
      enabled: true,
    })

    const report = await manager.sendGeneralInterrogation(endpoint, candidate, "unitlab")
    expect(report).toMatchObject({
      reason: "general-interrogation",
      sequenceNumber: 1,
      receivedAt: "2026-05-29T12:00:00.000Z",
    })
    expect(report.values.map(value => value.reference)).toEqual([
      "LD0/XCBR1.Pos.stVal[ST]",
      "LD0/PGGIO1.Ind1[ST]",
    ])

    await expect(manager.disableReportControl(endpoint, candidate, "unitlab")).resolves.toMatchObject({
      enabled: false,
    })
    await expect(manager.releaseReportControl(endpoint, candidate, "unitlab")).resolves.toMatchObject({
      reservedBy: null,
      owner: null,
    })
  })
})

function firstReportCandidate() {
  const model = parseScdSource({
    fileName: "report.scd",
    contentHash: "fixture",
    xmlText: reportScd,
  })
  const candidate = model.reportSubscriptions[0]
  if (!candidate) {
    throw new Error("Report fixture did not produce a subscription candidate")
  }
  return candidate
}
