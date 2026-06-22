#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_RUNTIME_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "server/native_wire_signal_runtime.h"

typedef enum {
    UNITLAB_NATIVE_SESSION_PHASE_IDLE = 0,
    UNITLAB_NATIVE_SESSION_PHASE_CONNECTING,
    UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED,
    UNITLAB_NATIVE_SESSION_PHASE_DISCOVERING,
    UNITLAB_NATIVE_SESSION_PHASE_DISCOVERED,
    UNITLAB_NATIVE_SESSION_PHASE_SUBSCRIBING,
    UNITLAB_NATIVE_SESSION_PHASE_REPORTING,
    UNITLAB_NATIVE_SESSION_PHASE_RECONNECTING,
    UNITLAB_NATIVE_SESSION_PHASE_DEGRADED,
    UNITLAB_NATIVE_SESSION_PHASE_FAILED,
    UNITLAB_NATIVE_SESSION_PHASE_CLOSED
} UnitLabNativeSessionPhase;

typedef enum {
    UNITLAB_NATIVE_SESSION_OPERATION_NONE = -1,
    UNITLAB_NATIVE_SESSION_OPERATION_CONNECT = 0,
    UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER,
    UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE,
    UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT
} UnitLabNativeSessionOperationKind;

typedef struct {
    char session_id[128U];
    char endpoint_id[160U];
    char device_key[160U];
    char rcb_key[160U];
    uint64_t connection_generation;
} UnitLabNativeSessionIdentity;

typedef struct {
    int wants_discovery;
    int wants_subscription;
    int wants_gi;
} UnitLabNativeSessionIntent;

typedef struct {
    int endpoint_connected;
    int discovery_available;
    int subscription_active;
    int reporting_active;
} UnitLabNativeSessionDesiredState;

typedef struct {
    char snapshot_id[96U];
    char endpoint_id[160U];
    char device_key[160U];
    char source_hash[96U];
    uint64_t created_at_ms;
    size_t logical_device_count;
    size_t logical_node_count;
    size_t data_set_count;
    size_t data_set_member_count;
    size_t report_control_count;
    size_t signal_count;
} UnitLabNativeDiscoverySnapshot;

typedef struct {
    UnitLabNativeSessionPhase phase;
    int associated;
    int discovered;
    int subscribed;
    int reporting;
    int connect_in_flight;
    int discover_in_flight;
    int subscribe_in_flight;
    int reconnect_in_flight;
    uint64_t last_report_timestamp_ms;
    char last_error_code[64U];
    char last_error_message[256U];
} UnitLabNativeSessionLiveState;

typedef struct UnitLabNativeSessionWorker UnitLabNativeSessionWorker;

typedef struct {
    char session_id[128U];
    char endpoint_id[160U];
    char device_key[160U];
    char phase[32U];
    char last_error_code[64U];
    char last_error_message[256U];
    int associated;
    int discovered;
    int subscribed;
    int reporting;
    int desired_endpoint_connected;
    int desired_discovery_available;
    int desired_subscription_active;
    int desired_reporting_active;
    int wants_subscription;
    int wants_gi;
    uint64_t connection_generation;
    uint64_t last_report_timestamp_ms;
    int has_discovery_snapshot;
    char snapshot_id[96U];
    size_t logical_device_count;
    size_t logical_node_count;
    size_t data_set_count;
    size_t data_set_member_count;
    size_t report_control_count;
    size_t signal_count;
    /* Runtime signal cache summary, independent from discovery counts. */
    size_t signal_cache_count;
    size_t live_signal_count;
    size_t stale_signal_count;
    size_t unknown_signal_count;
    uint64_t stale_generation_drop_count;
} UnitLabNativeSessionStatus;

typedef struct UnitLabNativeClientSessionState UnitLabNativeClientSessionState;

typedef struct {
    UnitLabNativeSessionIdentity identity;
    UnitLabNativeSessionDesiredState desired;
    UnitLabNativeSessionIntent intent;
    UnitLabNativeSessionLiveState live;
    UnitLabNativeDiscoverySnapshot discovery_snapshot;
    UnitLabNativeSignalRuntime signal_runtime;
    int has_discovery_snapshot;
} UnitLabNativeSessionRuntime;

typedef struct {
    UnitLabNativeSessionRuntime* items;
    UnitLabNativeSessionWorker** worker_owners;
    size_t item_count;
    size_t item_capacity;
    size_t worker_owner_capacity;
} UnitLabNativeSessionManager;

void unitlab_native_session_manager_init(UnitLabNativeSessionManager* manager);
void unitlab_native_session_manager_reset(UnitLabNativeSessionManager* manager);
UnitLabNativeSessionRuntime* unitlab_native_session_manager_get_or_create(
    UnitLabNativeSessionManager* manager,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key);

void unitlab_native_session_runtime_init(UnitLabNativeSessionRuntime* runtime);
void unitlab_native_session_runtime_set_identity(
    UnitLabNativeSessionRuntime* runtime,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key);
void unitlab_native_session_runtime_set_desired_state(
    UnitLabNativeSessionRuntime* runtime,
    int endpoint_connected,
    int discovery_available,
    int subscription_active,
    int reporting_active);
void unitlab_native_session_runtime_set_subscription_intent(
    UnitLabNativeSessionRuntime* runtime,
    const char* rcb_key,
    int wants_subscription,
    int wants_gi);
int unitlab_native_session_runtime_operation_is_in_flight(
    const UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind operation_kind);
int unitlab_native_session_runtime_begin_operation(
    UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind operation_kind);
void unitlab_native_session_runtime_complete_operation(
    UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind operation_kind,
    int success,
    const char* error_code,
    const char* error_message);
void unitlab_native_session_runtime_mark_report_received(
    UnitLabNativeSessionRuntime* runtime,
    uint64_t timestamp_ms);
void unitlab_native_session_runtime_apply_last_report_to_signals(
    UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeClientSessionState* session,
    uint64_t timestamp_ms);
void unitlab_native_session_runtime_mark_report_health_stale(
    UnitLabNativeSessionRuntime* runtime,
    const char* report_health_reason);
void unitlab_native_session_runtime_update_discovery_snapshot(
    UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeDiscoverySnapshot* snapshot);
void unitlab_native_session_runtime_mark_degraded(
    UnitLabNativeSessionRuntime* runtime,
    const char* error_code,
    const char* error_message);
void unitlab_native_session_runtime_mark_failed(
    UnitLabNativeSessionRuntime* runtime,
    const char* error_code,
    const char* error_message);
void unitlab_native_session_runtime_mark_closed(UnitLabNativeSessionRuntime* runtime);
const char* unitlab_native_session_phase_label(UnitLabNativeSessionPhase phase);
int unitlab_native_session_runtime_next_desired_operation(
    const UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind* operation_kind);
int unitlab_native_session_runtime_copy_status(
    const UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionStatus* status);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_RUNTIME_H */
