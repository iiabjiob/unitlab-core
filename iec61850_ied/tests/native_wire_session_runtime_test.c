#include "server/native_wire_session_runtime.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

static int expect_status(const UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionPhase phase, const char* error_code)
{
    UnitLabNativeSessionStatus status;

    if (!expect_true(unitlab_native_session_runtime_copy_status(runtime, &status), "copy_status failed")) {
        return 0;
    }
    if (!expect_true(strcmp(status.phase, unitlab_native_session_phase_label(phase)) == 0, "unexpected session phase")) {
        return 0;
    }
    if (!expect_true(strcmp(status.last_error_code, error_code) == 0, "unexpected error code")) {
        return 0;
    }
    return 1;
}

int main(void)
{
    UnitLabNativeSessionManager manager;
    UnitLabNativeSessionRuntime* runtime;
    UnitLabNativeDiscoverySnapshot snapshot;
    UnitLabNativeSessionOperationKind next_operation;
    uint64_t connect_generation;
    uint64_t reconnect_generation;

    unitlab_native_session_manager_init(&manager);
    runtime = unitlab_native_session_manager_get_or_create(&manager, "session-a", "mms:IED1@127.0.0.1:102", "IED1");
    if (!expect_true(runtime != NULL, "runtime allocation failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_status(runtime, UNITLAB_NATIVE_SESSION_PHASE_IDLE, "SESSION_RUNTIME_OK")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    unitlab_native_session_runtime_set_desired_state(runtime, 1, 1, 1, 1);
    if (!expect_true(unitlab_native_session_runtime_next_desired_operation(runtime, &next_operation), "next desired operation missing")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(next_operation == UNITLAB_NATIVE_SESSION_OPERATION_CONNECT, "next desired operation should connect first")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    {
        UnitLabNativeSessionStatus status;
        if (!expect_true(unitlab_native_session_runtime_copy_status(runtime, &status), "copy_status failed")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
        if (!expect_true(status.desired_endpoint_connected == 1 && status.desired_discovery_available == 1 && status.desired_subscription_active == 1 && status.desired_reporting_active == 1, "desired state not recorded")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
        if (!expect_true(status.associated == 0 && status.reporting == 0, "actual state should remain idle")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
    }

    if (!expect_true(unitlab_native_session_runtime_begin_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_CONNECT), "connect begin failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    connect_generation = runtime->identity.connection_generation;
    if (!expect_true(unitlab_native_session_runtime_begin_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_CONNECT), "duplicate connect should collapse")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(runtime->identity.connection_generation == connect_generation, "duplicate connect changed generation")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    unitlab_native_session_runtime_complete_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_CONNECT, 1, NULL, NULL);
    if (!expect_status(runtime, UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED, "SESSION_RUNTIME_OK")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(runtime->live.associated, "connect did not mark associated")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    if (!expect_true(unitlab_native_session_runtime_begin_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER), "discover begin failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snprintf(snapshot.snapshot_id, sizeof(snapshot.snapshot_id), "%s", "snapshot-a");
    snprintf(snapshot.endpoint_id, sizeof(snapshot.endpoint_id), "%s", "mms:IED1@127.0.0.1:102");
    snprintf(snapshot.device_key, sizeof(snapshot.device_key), "%s", "IED1PROT");
    snapshot.created_at_ms = 1234U;
    snapshot.logical_device_count = 1U;
    snapshot.logical_node_count = 2U;
    snapshot.data_set_count = 3U;
    snapshot.data_set_member_count = 4U;
    snapshot.report_control_count = 5U;
    snapshot.signal_count = 6U;
    unitlab_native_session_runtime_update_discovery_snapshot(runtime, &snapshot);
    unitlab_native_session_runtime_complete_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER, 1, NULL, NULL);
    if (!expect_status(runtime, UNITLAB_NATIVE_SESSION_PHASE_DISCOVERED, "SESSION_RUNTIME_OK")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(runtime->has_discovery_snapshot, "discovery snapshot missing")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(runtime->discovery_snapshot.logical_node_count == 2U && runtime->discovery_snapshot.signal_count == 6U, "discovery snapshot counters incorrect")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    unitlab_native_session_runtime_set_subscription_intent(runtime, "IED1LD0/LLN0.brcbA", 1, 1);
    if (!expect_true(unitlab_native_session_runtime_begin_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE), "subscribe begin failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    unitlab_native_session_runtime_complete_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE, 1, NULL, NULL);
    if (!expect_status(runtime, UNITLAB_NATIVE_SESSION_PHASE_REPORTING, "SESSION_RUNTIME_OK")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    unitlab_native_session_runtime_mark_report_received(runtime, 4567U);
    if (!expect_true(runtime->live.reporting && runtime->live.last_report_timestamp_ms == 4567U, "report timestamp not updated")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    unitlab_native_session_runtime_mark_closed(runtime);
    if (!expect_status(runtime, UNITLAB_NATIVE_SESSION_PHASE_CLOSED, "SESSION_RUNTIME_OK")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(unitlab_native_session_runtime_next_desired_operation(runtime, &next_operation), "next desired operation missing after close")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(next_operation == UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT, "closed session should reconnect when desired state is still active")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(unitlab_native_session_runtime_begin_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT), "reconnect begin failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    reconnect_generation = runtime->identity.connection_generation;
    if (!expect_true(unitlab_native_session_runtime_begin_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT), "duplicate reconnect should collapse")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(runtime->identity.connection_generation == reconnect_generation, "duplicate reconnect changed generation")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    unitlab_native_session_runtime_complete_operation(runtime, UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT, 1, NULL, NULL);
    if (!expect_status(runtime, UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED, "SESSION_RUNTIME_OK")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    {
        UnitLabNativeSessionManager failed_manager;
        UnitLabNativeSessionRuntime* failed_runtime;

        unitlab_native_session_manager_init(&failed_manager);
        failed_runtime = unitlab_native_session_manager_get_or_create(&failed_manager, "session-b", "mms:IED2@127.0.0.1:102", "IED2");
        if (!expect_true(failed_runtime != NULL, "failed runtime allocation failed")) {
            unitlab_native_session_manager_reset(&manager);
            unitlab_native_session_manager_reset(&failed_manager);
            return 1;
        }
        if (!expect_true(unitlab_native_session_runtime_begin_operation(failed_runtime, UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER), "failed discover begin failed")) {
            unitlab_native_session_manager_reset(&manager);
            unitlab_native_session_manager_reset(&failed_manager);
            return 1;
        }
        unitlab_native_session_runtime_complete_operation(
            failed_runtime,
            UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER,
            0,
            "NATIVE_WIRE_CLIENT_DISCOVER_FAILED",
            "discovery failed");
        if (!expect_status(failed_runtime, UNITLAB_NATIVE_SESSION_PHASE_DEGRADED, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED")) {
            unitlab_native_session_manager_reset(&manager);
            unitlab_native_session_manager_reset(&failed_manager);
            return 1;
        }
        unitlab_native_session_runtime_mark_closed(failed_runtime);
        if (!expect_status(failed_runtime, UNITLAB_NATIVE_SESSION_PHASE_CLOSED, "SESSION_RUNTIME_OK")) {
            unitlab_native_session_manager_reset(&manager);
            unitlab_native_session_manager_reset(&failed_manager);
            return 1;
        }
        unitlab_native_session_manager_reset(&failed_manager);
    }

    unitlab_native_session_manager_reset(&manager);
    printf("native session runtime test passed\n");
    return 0;
}
