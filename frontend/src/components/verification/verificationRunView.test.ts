import { describe, expect, it } from "vitest"

import { buildVerificationRunView } from "./verificationRunView"
import type { VerificationRunDetailResponse } from "@/types/verification"

function buildPassResult(): VerificationRunDetailResponse {
  return {
    test_run_id: "run-42",
    verification_run: {
      test_run_id: "run-42",
      verification_targets: [
        {
          signal_id: 101,
          signal_reference: "Breaker Close",
          signal_path: "breaker_close",
          endpoint_id: "sim:DO-002/unknown",
          expected_feedback_path: "kint_5",
          timeout_ms: 5000,
          window_ms: 1000,
          protocol_metadata: {},
          coverage_state: "exact",
          allocation_id: 1,
          unit_id: "DO-002",
        },
      ],
      subscription_plan: {
        plan_id: "plan-1",
        selected_signal_ids: [101],
        targets: [],
        groups: [],
        uncovered_targets: [],
        planning_diagnostics: [],
        coverage: {
          total_targets: 1,
          covered_targets: 1,
          partially_covered_targets: 0,
          uncovered_targets: 0,
          groups_count: 1,
          endpoints_count: 1,
          planning_quality: "good",
        },
      },
      session_snapshots: [],
      evidence_set: {
        test_run_id: "run-42",
        evidence: [],
        summary: {
          evidence_count: 1,
          observed_count: 1,
          stale_count: 0,
          timeout_count: 0,
          invalid_count: 0,
          late_count: 0,
          out_of_window_count: 0,
          source_generation: null,
        },
        diagnostics: [],
      },
      execution_context: {
        project_id: 7,
        signal_list_revision_id: 2,
        planner_version: "test",
        runtime_version: "simulator",
        policy_version: "v1",
      },
      workflow_state: "completed",
      verdict_state: "pass",
      verification_confidence: "simulated_fallback",
      confidence_reason: "fallback_planning_used",
      diagnostics: [],
      verification_steps: [
        {
          step_id: "step-101",
          signal_id: 101,
          target_index: 0,
          group_id: "group-1",
          step_state: "completed",
          expected_path: "kint_5",
          expected_window_ms: 1000,
          freshness: "live",
          evidence_status: "observed",
          verdict_state: "pass",
          evidence_ids: ["ev-1"],
          actual_report_path: "LD0/XCBR1.Pos.stVal",
          source_session_id: "run-42:sim:DO-002/unknown",
          source_generation: 1,
          source_report_rpt_id: "brcbA",
          source_report_dat_set: "ds-a",
          verification_confidence: "simulated_fallback",
          confidence_reason: "fallback_planning_used",
          triggered_at: null,
          observed_at: null,
          latency_ms: 43,
          reason: "report_received",
          diagnostics: [{ code: "report_received", message: "Report received within the verification window." }],
        },
      ],
    },
    verdict_explanation: {
      test_run_id: "run-42",
      verdict_state: "pass",
      verification_confidence: "simulated_fallback",
      confidence_reason: "fallback_planning_used",
      headline: "PASS",
      summary: "PASS: observed LD0/XCBR1.Pos.stVal on IED-A/P1 in 43 ms.",
      signals: [
        {
          signal_id: 101,
          signal_reference: "Breaker Close",
          signal_path: "breaker_close",
          expected_path: "LD0/XCBR1.Pos.stVal",
          observed_path: "LD0/XCBR1.Pos.stVal",
          source_ied: "IED-A",
          endpoint_id: "sim:DO-002/unknown",
          rpt_id: "brcbA",
          dataset: "ds-a",
          evidence_status: "observed",
          verdict_state: "pass",
          latency_ms: 43,
          reason: "expected feedback observed within the verification window",
          diagnostics: [{ code: "report_received", message: "Report received within the verification window." }],
        },
      ],
      diagnostics: [{ code: "report_received", message: "Report received within the verification window." }],
    },
  }
}

function buildFailResult(): VerificationRunDetailResponse {
  const result = buildPassResult()
  result.verdict_explanation.verdict_state = "fail"
  result.verdict_explanation.headline = "FAIL"
  result.verdict_explanation.summary = "FAIL: no confirmation arrived before the timeout expired."
  result.verdict_explanation.signals[0].evidence_status = "timeout"
  result.verdict_explanation.signals[0].verdict_state = "fail"
  result.verdict_explanation.signals[0].observed_path = null
  result.verdict_explanation.signals[0].latency_ms = null
  return result
}

describe("verificationRunView", () => {
  it("builds a PASS view model with explain-why fields", () => {
    const view = buildVerificationRunView(buildPassResult())

    expect(view?.headline).toBe("PASS")
    expect(view?.summary).toContain("43 ms")
    expect(view?.verificationConfidence).toBe("SIMULATED FALLBACK")
    expect(view?.confidenceReason).toBe("fallback_planning_used")
    expect(view?.signals[0]).toMatchObject({
      title: "Breaker Close",
      output: "breaker_close",
      expected: "LD0/XCBR1.Pos.stVal",
      observed: "LD0/XCBR1.Pos.stVal",
      unit: "DO-002",
      endpoint: "sim:DO-002/unknown",
      rcb: "brcbA",
      dataset: "ds-a",
      latency: "43 ms",
      reason: "expected feedback observed within the verification window",
      verificationConfidence: "SIMULATED FALLBACK",
      confidenceReason: "fallback_planning_used",
      verdictState: "PASS",
      evidenceStatus: "OBSERVED",
    })
  })

  it("builds a FAIL view model for timeout evidence", () => {
    const view = buildVerificationRunView(buildFailResult())

    expect(view?.headline).toBe("FAIL")
    expect(view?.summary).toContain("timeout expired")
    expect(view?.signals[0]).toMatchObject({
      observed: "—",
      latency: "—",
      verdictState: "FAIL",
      evidenceStatus: "TIMEOUT",
    })
  })
})
