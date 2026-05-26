import { describe, expect, it } from "vitest"

import { tryHttp } from "./http"

describe("http helpers", () => {
  it("wraps successful async operations", async () => {
    await expect(tryHttp(Promise.resolve({ id: 1 }))).resolves.toEqual({
      ok: true,
      data: { id: 1 },
    })
  })

  it("normalizes failed async operations", async () => {
    const result = await tryHttp(Promise.reject(new Error("Local failure")))

    expect(result).toMatchObject({
      ok: false,
      message: "Local failure",
      status: null,
    })
  })
})
