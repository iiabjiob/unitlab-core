# IEC 61850 External IED Discovery Pipeline

The external IED flow is structured as a staged backend pipeline. Each stage owns one decision boundary so discovery, planning, and verification can be rerun independently.

```text
Network Watcher
  -> Discovery Policy
  -> Discovery Scheduler
  -> Discovery State Machine
  -> Discovery Worker
  -> Discovery Cache
  -> Discovery Planner
  -> Verification Plan
```

## Ownership

- Network Watcher checks configured MMS endpoints and publishes endpoint status changes.
- Discovery Policy decides whether discovery is needed.
- Discovery Scheduler queues accepted `DiscoveryRequest` jobs and does not contain discovery business rules.
- Discovery State Machine owns explicit lifecycle transitions: queued, running, succeeded, failed, retry waiting, cancelled, and stale.
- Discovery Worker consumes `DiscoveryRequest`, opens the MMS association, performs discovery, builds the model, computes `ModelFingerprint`, and writes `DiscoveryResult`.
- Discovery Cache stores metadata and discovered model data separately from scheduler state.
- Discovery Planner consumes the Signal List and cached model data to build the Verification Plan without MMS communication.

## Non-Ownership

- The Discovery Worker does not update UI state directly.
- The Discovery Worker does not decide retries.
- The Discovery Worker does not schedule follow-up discovery.
- The Discovery Worker does not enable reports, reserve RCBs, or write MMS values.

## Recompute Rules

- Signal List changed: rerun planning only when the cached IED model is still valid.
- Cached IED model changed: rerun planning against the new cache.
- IED model missing, stale, or untrusted: rerun discovery, then planning.
- User clicked Run Verification: use the existing plan if ready; request discovery only when required by policy and lifecycle state.

## Scheduling Rules

- A stable reachable watcher update does not rediscover by itself when cache metadata and model data are valid.
- Missing cache, stale lifecycle state, missing model fingerprint, explicit refresh, or verification-required context may queue discovery.
- Planning fingerprint changes are planning input changes. They do not require MMS rediscovery by default.
- User refresh bypasses normal background dedupe. Verification-required requests may upgrade queued lower-priority background discovery.
- Running discovery jobs that exceed the configured timeout are recovered to retry waiting so they cannot block scheduling forever.
- Transient MMS/network failures use retry waiting with exponential backoff. Permanent parse/model errors remain failed.

## Planning Event

After successful discovery, the worker stores metadata and model data, then emits `ExternalIedDiscoveryCompleted` on the backend planning event stream. The event is a domain signal for planning consumers, not a UI update.
