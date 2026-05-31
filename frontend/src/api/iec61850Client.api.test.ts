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
})
