import { describe, expect, it } from "vitest"

import { HttpRequestError } from "./httpErrors"
import { toUserFacingErrorMessage } from "./errorMessages"

function httpError(overrides: Partial<ConstructorParameters<typeof HttpRequestError>[1]> = {}) {
  return new HttpRequestError("Request failed", {
    status: null,
    detail: "",
    url: "",
    method: "",
    code: "",
    message: "Request failed",
    responseData: null,
    isHttpError: true,
    ...overrides,
  })
}

describe("errorMessages", () => {
  it("maps active test-run conflicts", () => {
    expect(toUserFacingErrorMessage(httpError({
      status: 409,
      url: "/api/v1/workspaces/2/signal-allocations/test-run/jobs",
    }))).toBe("A test run is already active in this workspace. Stop it or wait until it finishes.")
  })

  it("maps known test-run size details", () => {
    expect(toUserFacingErrorMessage(httpError({
      status: 400,
      detail: "Too many signals for test run (max 20000)",
    }))).toBe("Too many signals selected for test run. Maximum is 20000. Reduce selection or run in batches.")
  })

  it("falls back to plain errors", () => {
    expect(toUserFacingErrorMessage(new Error("Local failure"))).toBe("Local failure")
  })
})
