import { describe, expect, it } from "vitest"

import { parseScdSource } from "../scd-sld-core"
import {
  buildIec61850ReportSubscriptionPlan,
  createIec61850SimulatorAdapter,
  Iec61850ReportManager,
  reportControlKey,
  toReportControlRef,
  type Iec61850DeviceEndpoint,
  type Iec61850ReportControlCandidate,
  type Iec61850ReportControlState,
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

  it("records simulator lifecycle transitions in a deterministic event log", async () => {
    const candidate = firstReportCandidate()
    const reference = toReportControlRef(candidate)
    const adapter = createIec61850SimulatorAdapter({
      devices: [{ endpoint, reports: [candidate] }],
      now: () => new Date("2026-05-29T12:00:00.000Z"),
    })

    const connection = await adapter.connect(endpoint)
    await expect(connection.readReportControl(reference)).resolves.toMatchObject({ lifecycleState: "read" })
    await expect(connection.reserveReportControl(reference, "unitlab")).resolves.toMatchObject({ lifecycleState: "reserved" })
    await expect(connection.enableReportControl(reference, "unitlab")).resolves.toMatchObject({ lifecycleState: "enabled" })
    await expect(connection.sendGeneralInterrogation(reference, "unitlab")).resolves.toMatchObject({
      sequenceNumber: 1,
      values: expect.any(Array),
    })
    await expect(connection.disableReportControl(reference, "unitlab")).resolves.toMatchObject({ lifecycleState: "disabled" })
    await expect(connection.releaseReportControl(reference, "unitlab")).resolves.toMatchObject({ lifecycleState: "released" })
    await connection.disconnect()

    expect(adapter.getEventLog().map(event => event.kind)).toEqual([
      "connect",
      "read",
      "reserve",
      "enable",
      "general-interrogation",
      "report",
      "disable",
      "release",
      "disconnect",
    ])
    expect(adapter.getEventLog().map(event => event.at)).toEqual(Array.from({ length: 9 }, () => "2026-05-29T12:00:00.000Z"))
  })

  it("fails invalid simulator state transitions with stable error codes", async () => {
    const candidate = firstReportCandidate()
    const reference = toReportControlRef(candidate)

    const enableWithoutReservation = createIec61850SimulatorAdapter({ devices: [{ endpoint, reports: [candidate] }] })
    await expect((await enableWithoutReservation.connect(endpoint)).enableReportControl(reference, "unitlab")).rejects.toMatchObject({
      code: "ENABLE_WITHOUT_RESERVATION",
    })

    const giWhileDisabled = createIec61850SimulatorAdapter({ devices: [{ endpoint, reports: [candidate] }] })
    await expect((await giWhileDisabled.connect(endpoint)).sendGeneralInterrogation(reference, "unitlab")).rejects.toMatchObject({
      code: "GI_WHILE_DISABLED",
    })

    const reservationConflict = createIec61850SimulatorAdapter({ devices: [{ endpoint, reports: [candidate] }] })
    const reservationConflictConnection = await reservationConflict.connect(endpoint)
    await reservationConflictConnection.reserveReportControl(reference, "unitlab")
    await expect(reservationConflictConnection.reserveReportControl(reference, "other-client")).rejects.toMatchObject({
      code: "RESERVATION_CONFLICT",
    })

    const staleConfRev = createIec61850SimulatorAdapter({
      devices: [{
        endpoint,
        reports: [candidate],
        overrides: {
          [reportControlKey(reference)]: { confRev: "8" } satisfies Partial<Iec61850ReportControlState>,
        },
      }],
    })
    const staleConfRevConnection = await staleConfRev.connect(endpoint)
    await staleConfRevConnection.reserveReportControl(reference, "unitlab")
    await expect(staleConfRevConnection.enableReportControl(reference, "unitlab")).rejects.toMatchObject({
      code: "CONFREV_STALE",
    })

    const enabledDisconnect = createIec61850SimulatorAdapter({
      devices: [{ endpoint, reports: [candidate] }],
      strictDisconnectWhileEnabled: true,
    })
    const enabledDisconnectConnection = await enabledDisconnect.connect(endpoint)
    await enabledDisconnectConnection.reserveReportControl(reference, "unitlab")
    await enabledDisconnectConnection.enableReportControl(reference, "unitlab")
    await expect(enabledDisconnectConnection.disconnect()).rejects.toMatchObject({
      code: "DISCONNECT_WHILE_ENABLED",
    })
    const enabledDisconnectEvents = enabledDisconnect.getEventLog()
    expect(enabledDisconnectEvents[enabledDisconnectEvents.length - 1]).toMatchObject({
      kind: "failure",
      lifecycleState: "failed",
      code: "DISCONNECT_WHILE_ENABLED",
    })
  })

  it("builds a deterministic subscription plan from selected full-path Signal List addresses", () => {
    const candidate = firstReportCandidate()
    const plan = buildIec61850ReportSubscriptionPlan({
      candidates: [candidate],
      selectedSignals: [
        { id: "sig-1", address: "IED1LD0/XCBR1/Pos/stVal[ST]" },
        { id: "sig-2", address: "IED1!IED1LD0/PGGIO1/Ind1[ST]" },
      ],
    })

    expect(plan).toMatchObject({
      selectedSignalCount: 2,
      matchedSignalCount: 2,
      unmatchedSignalCount: 0,
      ambiguousSignalCount: 0,
      requiredReportCount: 1,
    })
    expect(plan.devices).toHaveLength(1)
    expect(plan.devices[0]?.reports[0]?.candidate.reportControlName).toBe("brcbEvents")
    expect(plan.devices[0]?.reports[0]?.matchedSignals.map(signal => signal.modelReference)).toEqual([
      "LD0/PGGIO1.Ind1[ST]",
      "LD0/XCBR1.Pos.stVal[ST]",
    ])
    expect(plan.diagnostics).toEqual([])
  })

  it("marks duplicate, unmatched, ambiguous, parent FCD, and multi-report selected signals", () => {
    const candidate = firstReportCandidate()
    const secondReport = cloneCandidate(candidate, {
      id: "IED1/AP1/LD0/LLN0/urcbEvents/subscription",
      reportControlName: "urcbEvents",
      reportKind: "unbuffered",
    })
    const secondIed = cloneCandidate(candidate, {
      id: "IED2/AP1/LD0/LLN0/brcbEvents/subscription",
      iedName: "IED2",
    })

    const plan = buildIec61850ReportSubscriptionPlan({
      candidates: [candidate, secondReport, secondIed],
      selectedSignals: [
        { id: "exact", address: "IED1LD0/XCBR1/Pos/stVal[ST]" },
        { id: "duplicate", address: "IED1LD0/XCBR1.Pos.stVal[ST]" },
        { id: "parent", address: "IED1LD0/XCBR1/Pos/stVal/q[ST]" },
        { id: "ambiguous", address: "LD0/XCBR1.Pos.stVal[ST]" },
        { id: "missing", address: "IED1LD0/XCBR1/Pos/Oper.ctlVal[CO]" },
      ],
    })

    expect(plan.matchedSignalCount).toBe(3)
    expect(plan.unmatchedSignalCount).toBe(1)
    expect(plan.ambiguousSignalCount).toBe(1)
    expect(plan.requiredReportCount).toBe(2)
    expect(plan.diagnostics.map(diagnostic => diagnostic.code)).toEqual([
      "MULTIPLE_REPORT_CANDIDATES",
      "DUPLICATE_SELECTED_SIGNAL",
      "MULTIPLE_REPORT_CANDIDATES",
      "FCD_PARENT_MATCH",
      "MULTIPLE_REPORT_CANDIDATES",
      "SIGNAL_AMBIGUOUS",
      "SIGNAL_NOT_FOUND",
    ])
    expect(plan.ambiguousSignals[0]?.candidates.map(candidate => candidate.iedName)).toEqual(["IED1", "IED2"])
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

function cloneCandidate(
  candidate: Iec61850ReportControlCandidate,
  overrides: Partial<Iec61850ReportControlCandidate>,
): Iec61850ReportControlCandidate {
  return {
    ...candidate,
    ...overrides,
    signals: [...candidate.signals],
    triggerOptions: { ...candidate.triggerOptions },
    optionalFields: { ...candidate.optionalFields },
  }
}
