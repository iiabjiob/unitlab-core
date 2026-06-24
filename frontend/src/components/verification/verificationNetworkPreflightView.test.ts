import { describe, expect, it } from "vitest"

import { buildVerificationNetworkPreflightView } from "./verificationNetworkPreflightView"
import type { VerificationNetworkPreflightResponse } from "@/types/verification"

function buildReadyResponse(): VerificationNetworkPreflightResponse {
  return {
    preflight: {
      workspace_id: 7,
      test_run_id: "vr-1",
      requested_runtime_version: "simulator",
      recommended_runtime_version: "mms",
      overall_state: "ready",
      overall_hint: "Real MMS verification is ready; switch the run to MMS to use the live BCU report path.",
      groups: [
        {
          group_id: "group-1",
          endpoint_id: "KINTE15BCU01/P1",
          ied_name: "KINTE15BCU01",
          access_point_name: "P1",
          target_host: "10.10.10.250",
          target_port: 102,
          readiness_state: "ready",
          operator_hint: "Real MMS is ready: eth0 (10.10.10.20/255.255.255.0) can reach 10.10.10.250:102.",
          recommended_interface_name: "eth0",
          recommended_local_ip: "10.10.10.20",
          recommended_netmask: "255.255.255.0",
          observed_ips: ["10.10.10.250"],
          observed_network_hints: ["subnet=10.10.10.0/24"],
          interfaces: [],
          diagnostics: [],
        },
      ],
      diagnostics: [],
    },
  }
}

describe("verificationNetworkPreflightView", () => {
  it("builds a readable preflight summary", () => {
    const view = buildVerificationNetworkPreflightView(buildReadyResponse())

    expect(view).toEqual({
      headline: "Network ready for real MMS",
      summary: "Real MMS verification is ready; switch the run to MMS to use the live BCU report path.",
      stateTone: "ready",
      runtimeSummary: "SIMULATOR requested, MMS recommended",
      groupHints: ["Real MMS is ready: eth0 (10.10.10.20/255.255.255.0) can reach 10.10.10.250:102."],
    })
  })

  it("returns null when no preflight result is available", () => {
    expect(buildVerificationNetworkPreflightView(null)).toBeNull()
  })
})
