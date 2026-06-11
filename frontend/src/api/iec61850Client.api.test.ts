import { beforeEach, describe, expect, it, vi } from "vitest"

const httpDataMock = vi.hoisted(() => ({
  get: vi.fn(),
  post: vi.fn(),
}))

vi.mock("./http", () => ({
  httpData: httpDataMock,
}))

import { Iec61850ClientAPI } from "./iec61850Client.api"

describe("Iec61850ClientAPI", () => {
  beforeEach(() => {
    httpDataMock.get.mockReset()
    httpDataMock.post.mockReset()
  })

  it("calls the client state endpoint", async () => {
    httpDataMock.get.mockResolvedValue({ session_open: false })

    await Iec61850ClientAPI.state()

    expect(httpDataMock.get).toHaveBeenCalledWith("/api/v1/iec61850/client/state")
  })

  it("calls the client control session endpoint", async () => {
    httpDataMock.post.mockResolvedValue({ session_open: true })

    await Iec61850ClientAPI.openSession()

    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/session/open")
  })
  it("calls the IEDScout-shaped debug control endpoints", async () => {
    httpDataMock.post.mockResolvedValue({ session_open: true })

    await Iec61850ClientAPI.discoverIed()
    await Iec61850ClientAPI.connectIed()
    await Iec61850ClientAPI.disconnectIed()
    await Iec61850ClientAPI.closeIed()
    await Iec61850ClientAPI.enableReporting()
    await Iec61850ClientAPI.sendGeneralInterrogation()

    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/ied/discover")
    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/ied/connect")
    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/ied/disconnect")
    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/ied/close")
    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/report-control/rptena")
    expect(httpDataMock.post).toHaveBeenCalledWith("/api/v1/iec61850/client/report-control/gi")
  })

})
