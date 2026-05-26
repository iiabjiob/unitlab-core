import { describe, expect, it } from "vitest"

import {
  getHttpErrorContext,
  HttpRequestError,
  normalizeHttpDetail,
} from "./httpErrors"

describe("httpErrors", () => {
  it("normalizes FastAPI detail payloads", () => {
    expect(normalizeHttpDetail(" Plain detail ")).toBe("Plain detail")
    expect(normalizeHttpDetail([{ msg: "first" }, { msg: "second" }])).toBe("first; second")
    expect(normalizeHttpDetail({ message: "Object message" })).toBe("Object message")
  })

  it("extracts context from axios-shaped errors without importing axios at call sites", () => {
    const context = getHttpErrorContext({
      code: "ERR_NETWORK",
      message: "Network Error",
      config: { method: "get", url: "/api/v1/devices" },
      response: {
        status: 503,
        data: { detail: "Service unavailable" },
      },
    })

    expect(context).toMatchObject({
      status: 503,
      detail: "Service unavailable",
      url: "/api/v1/devices",
      method: "GET",
      code: "ERR_NETWORK",
      isHttpError: true,
    })
  })

  it("preserves normalized request errors", () => {
    const error = new HttpRequestError("Mapped message", {
      status: 409,
      detail: "Conflict",
      url: "/api/v1/jobs",
      method: "POST",
      code: "",
      message: "Conflict",
      responseData: { detail: "Conflict" },
      isHttpError: true,
    })

    expect(getHttpErrorContext(error)).toMatchObject({
      status: 409,
      detail: "Conflict",
      message: "Mapped message",
      isHttpError: true,
    })
  })
})
