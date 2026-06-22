# UnitLab recovery and freshness contract

Status: draft recovery contract for the Python/FastAPI product layer.

This document defines how the product layer should react when an IEC 61850 session becomes unavailable, stale, or needs to be recovered.

## Ownership

### Python/FastAPI product layer

Owns:
- recovery policy;
- retry / reconnect decisions;
- stale verdict policy;
- evidence preservation across reconnects;
- user-visible degraded state.

### C IEC 61850 runtime

Provides:
- session generation tracking;
- reconnect-capable runtime state;
- signal freshness;
- stale-generation rejection;
- diagnostics.

The C runtime should not decide product-level retry policy.

## Recovery triggers

The product layer should consider recovery when one or more of these occur:
- transport disconnect;
- association loss;
- report-health degradation;
- stale-generation update;
- subscription loss;
- explicit user reconnect;
- runtime failure that invalidates the current live session.

## Required recovery behavior

When recovery is required, the product layer should:
- preserve the existing desired plan;
- preserve the original verification targets;
- preserve prior evidence;
- preserve per-IED / per-session isolation when a group spans multiple devices;
- request a new session generation if reconnect is performed;
- mark previously live signals or evidence as stale when their generation is no longer valid;
- suppress duplicate reconnect attempts for the same active session.

## Freshness rules

### Live

A signal or evidence item is `live` only while its source session and generation are still current.

### Stale

A signal or evidence item becomes `stale` when:
- its source session disconnects;
- its generation is no longer current;
- report health indicates the underlying source is no longer valid for confirmation;
- the product layer explicitly invalidates the old runtime generation during reconnect.

### Recovered

A new valid report update after reconnect should:
- move the signal from `stale` back to `live`;
- update source generation and provenance;
- preserve the last known previous evidence as historical context.

## Duplicate reconnect rules

- Only one reconnect operation should be active for a session at a time.
- Additional reconnect requests should join or no-op against the active recovery operation.
- Recovery should be idempotent from the product layer point of view.
- Late frames from an old generation must never overwrite the current generation.

## Evidence rules

- Stale evidence must remain visible and explainable.
- A reconnect must not erase prior confirmation/verification evidence.
- A new confirmation after reconnect should be recorded as new evidence, not as a rewrite of the old evidence.
- Old-generation evidence may remain in the durable history, but it must not be treated as current live evidence.

## Suggested state transitions

- `running` -> `stale`
- `running` -> `reconnecting`
- `running` -> `degraded`
- `reconnecting` -> `discovering`
- `discovering` -> `subscribing`
- `subscribing` -> `running`
- `running` -> `failed`
- `running` -> `aborted`

## Failure reasons

The product layer should distinguish:
- `disconnect`
- `association_lost`
- `report_health_degraded`
- `stale_generation`
- `subscription_lost`
- `timeout`
- `user_reconnect`
- `runtime_failure`

## Validation expectations

- A reconnect should keep the same verification intent unless the user explicitly changes it.
- Old-generation report updates must not change live evidence.
- Stale signals should keep their last known good value while being visibly non-live.
- Reconnect should be visible in product state as a separate recovery phase.
- One IED recovery must not invalidate other IED sessions in the same selected group.
