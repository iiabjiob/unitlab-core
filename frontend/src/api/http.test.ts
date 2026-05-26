import { afterEach, describe, expect, it, vi } from "vitest"

import { httpData, tryHttp } from "./http"

afterEach(() => {
  vi.unstubAllGlobals()
})

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

  it("reads JSON responses through fetch", async () => {
    const fetchMock = vi.fn(async () => new Response(JSON.stringify({ status: "online" }), {
      status: 200,
      headers: { "content-type": "application/json" },
    }))
    vi.stubGlobal("fetch", fetchMock)

    await expect(httpData.get<{ status: string }>("/api/v1/health")).resolves.toEqual({ status: "online" })
    expect(fetchMock).toHaveBeenCalledWith("/api/v1/health", expect.objectContaining({
      method: "GET",
      cache: "no-store",
    }))
  })

  it("throws normalized HTTP errors for non-OK responses", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ detail: "Service unavailable" }), {
      status: 503,
      statusText: "Service Unavailable",
      headers: { "content-type": "application/json" },
    })))

    await expect(httpData.get("/api/v1/health")).rejects.toMatchObject({
      status: 503,
      detail: "Service unavailable",
      message: "Server error occurred. Please try again in a moment.",
    })
  })

  it("lets fetch set multipart boundaries for FormData", async () => {
    const fetchMock = vi.fn(async () => new Response("{}", {
      status: 200,
      headers: { "content-type": "application/json" },
    }))
    vi.stubGlobal("fetch", fetchMock)

    await httpData.post("/api/v1/import", new FormData(), {
      headers: { "Content-Type": "multipart/form-data" },
    })

    const calls = fetchMock.mock.calls as unknown as Array<[RequestInfo | URL, RequestInit | undefined]>
    const init = calls[0]?.[1] as RequestInit
    expect(init.headers).toBeInstanceOf(Headers)
    expect((init.headers as Headers).has("Content-Type")).toBe(false)
  })
})
