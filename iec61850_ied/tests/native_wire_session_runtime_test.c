#include "server/native_wire_session_runtime.h"
#include "server/native_wire_client_session.h"

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

static int test_report_sequence_policy_tracks_gaps_duplicates_and_generation_reset(void)
{
    UnitLabNativeClientSessionState session;
    UnitLabNativeReportSequenceDisposition disposition;

    memset(&session, 0, sizeof(session));
    disposition = unitlab_native_client_session_observe_report_sequence(&session, 1U, 10U, 0, 0U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED, "first report sequence should be accepted")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.has_last_report_sequence_number == 1 && session.subscription_model.last_report_sequence_number == 10U, "first report sequence not recorded")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.has_last_report_sub_sequence_number == 0 && strcmp(session.subscription_model.report_health, "live") == 0, "first report sequence health not recorded")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_generation == 1U, "first report sequence generation not recorded")) {
        return 0;
    }

    disposition = unitlab_native_client_session_observe_report_sequence(&session, 1U, 10U, 0, 0U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_DUPLICATE, "duplicate report sequence should be rejected")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.report_sequence_duplicate_count == 1U && session.subscription_model.report_sequence_drop_count == 1U, "duplicate report sequence counters not updated")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_number == 10U, "duplicate report sequence changed last sequence")) {
        return 0;
    }
    if (!expect_true(strcmp(session.subscription_model.report_health, "degraded") == 0, "duplicate report sequence health not degraded")) {
        return 0;
    }

    disposition = unitlab_native_client_session_observe_report_sequence(&session, 1U, 9U, 0, 0U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_OUT_OF_ORDER, "out-of-order report sequence should be rejected")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.report_sequence_out_of_order_count == 1U && session.subscription_model.report_sequence_drop_count == 2U, "out-of-order report sequence counters not updated")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_number == 10U, "out-of-order report sequence changed last sequence")) {
        return 0;
    }

    disposition = unitlab_native_client_session_observe_report_sequence(&session, 1U, 13U, 0, 0U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_GAP, "gap report sequence should be accepted with gap state")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.report_sequence_gap_count == 1U && session.subscription_model.report_sequence_missing_count == 2U, "gap report sequence counters not updated")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_number == 13U, "gap report sequence did not advance")) {
        return 0;
    }
    if (!expect_true(strcmp(session.subscription_model.report_health, "degraded") == 0, "gap report sequence health not degraded")) {
        return 0;
    }

    disposition = unitlab_native_client_session_observe_report_sequence(&session, 1U, 13U, 1, 1U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED, "subsequence report should be accepted")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.has_last_report_sub_sequence_number == 1 && session.subscription_model.last_report_sub_sequence_number == 1U, "subsequence report not recorded")) {
        return 0;
    }
    if (!expect_true(strcmp(session.subscription_model.report_health, "live") == 0, "subsequence report health not live")) {
        return 0;
    }

    disposition = unitlab_native_client_session_observe_report_sequence(&session, 2U, 1U, 0, 0U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED, "new generation report sequence should be accepted")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_generation == 2U, "new generation report sequence generation not recorded")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_number == 1U, "new generation report sequence not reset")) {
        return 0;
    }

    session.subscription_model.last_report_sequence_generation = 2U;
    session.subscription_model.last_report_sequence_number = UINT32_MAX;
    session.subscription_model.has_last_report_sequence_number = 1;
    disposition = unitlab_native_client_session_observe_report_sequence(&session, 2U, 0U, 0, 0U);
    if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED, "wrap-around report sequence should be accepted")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.report_sequence_wrap_count == 1U, "wrap-around report sequence counter not updated")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.last_report_sequence_number == 0U, "wrap-around report sequence not reset to zero")) {
        return 0;
    }

    {
        UnitLabNativeDiscoveredRcb* report_rcb_a;
        UnitLabNativeDiscoveredRcb* report_rcb_b;

        report_rcb_a = unitlab_native_client_session_append_discovered_rcb(&session, "RCB-A", "LLN0$BR$brcbA01");
        report_rcb_b = unitlab_native_client_session_append_discovered_rcb(&session, "RCB-B", "LLN0$BR$brcbB01");
        if (!expect_true(report_rcb_a != NULL && report_rcb_b != NULL, "per-RCB sequence state allocation failed")) {
            return 0;
        }
        snprintf(session.subscription_model.rcb_domain, sizeof(session.subscription_model.rcb_domain), "%s", "RCB-A");
        snprintf(session.subscription_model.rcb_item, sizeof(session.subscription_model.rcb_item), "%s", "LLN0$BR$brcbA01");
        disposition = unitlab_native_client_session_observe_report_sequence(&session, 3U, 40U, 0, 0U);
        if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED, "RCB A report should be accepted")) {
            return 0;
        }
        snprintf(session.subscription_model.rcb_domain, sizeof(session.subscription_model.rcb_domain), "%s", "RCB-B");
        snprintf(session.subscription_model.rcb_item, sizeof(session.subscription_model.rcb_item), "%s", "LLN0$BR$brcbB01");
        disposition = unitlab_native_client_session_observe_report_sequence(&session, 3U, 7U, 0, 0U);
        if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED, "RCB B report should be accepted")) {
            return 0;
        }
        snprintf(session.subscription_model.rcb_domain, sizeof(session.subscription_model.rcb_domain), "%s", "RCB-A");
        snprintf(session.subscription_model.rcb_item, sizeof(session.subscription_model.rcb_item), "%s", "LLN0$BR$brcbA01");
        disposition = unitlab_native_client_session_observe_report_sequence(&session, 2U, 41U, 0, 0U);
        if (!expect_true(disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_OUT_OF_ORDER, "stale generation should be rejected")) {
            return 0;
        }
        if (!expect_true(strcmp(session.subscription_model.report_health, "degraded") == 0, "stale generation did not degrade health")) {
            return 0;
        }
        unitlab_native_client_session_reset_report_sequence(&session);
        if (!expect_true(report_rcb_a->has_last_report_sequence_number == 0, "RCB A reset did not clear selected state")) {
            return 0;
        }
        if (!expect_true(report_rcb_b->has_last_report_sequence_number == 1 && report_rcb_b->last_report_sequence_number == 7U, "RCB B state should remain independent across reconnect")) {
            return 0;
        }
    }

    unitlab_native_client_session_reset_report_sequence(&session);
    if (!expect_true(session.subscription_model.has_last_report_sequence_number == 0 && session.subscription_model.last_report_sequence_generation == 0U, "report sequence reset did not clear state")) {
        return 0;
    }
    if (!expect_true(session.subscription_model.report_sequence_gap_count == 0U && session.subscription_model.report_sequence_duplicate_count == 0U && session.subscription_model.report_sequence_out_of_order_count == 0U && session.subscription_model.report_sequence_drop_count == 0U && session.subscription_model.report_sequence_missing_count == 0U && session.subscription_model.report_sequence_wrap_count == 0U, "report sequence reset did not clear counters")) {
        return 0;
    }
    if (!expect_true(strcmp(session.subscription_model.report_health, "unknown") == 0, "report sequence reset did not restore health unknown")) {
        return 0;
    }
    return 1;
}

static int test_report_health_staleness_updates_live_signal_cache(void)
{
    UnitLabNativeSessionManager manager;
    UnitLabNativeSessionRuntime* runtime;
    UnitLabNativeSignalUpdate signal_update;
    UnitLabNativeSignalChange signal_change;
    const UnitLabNativeSignalState* signal_state;

    unitlab_native_session_manager_init(&manager);
    runtime = unitlab_native_session_manager_get_or_create(&manager, "session-c", "mms:IED3@127.0.0.1:102", "IED3");
    if (!expect_true(runtime != NULL, "staleness runtime allocation failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 0;
    }
    unitlab_native_signal_runtime_set_current_connection_generation(&runtime->signal_runtime, 3U);
    memset(&signal_update, 0, sizeof(signal_update));
    snprintf(signal_update.signal_path, sizeof(signal_update.signal_path), "%s", "IED3LD0/XCBR1.Pos");
    snprintf(signal_update.data_reference, sizeof(signal_update.data_reference), "%s", "IED3LD0/XCBR1$ST$Pos$stVal");
    snprintf(signal_update.display_reference, sizeof(signal_update.display_reference), "%s", "IED3LD0/XCBR1.Pos.stVal");
    snprintf(signal_update.leaf_name, sizeof(signal_update.leaf_name), "%s", "stVal");
    snprintf(signal_update.value_summary, sizeof(signal_update.value_summary), "%s", "true");
    signal_update.leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE;
    signal_update.value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL;
    signal_update.bool_value = 1;
    signal_update.observed_at_ms = 1000U;
    signal_update.source_connection_generation = 3U;
    signal_state = unitlab_native_signal_runtime_apply_update(&runtime->signal_runtime, &signal_update, &signal_change);
    if (!expect_true(signal_state != NULL && signal_state->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE, "signal should start live")) {
        unitlab_native_session_manager_reset(&manager);
        return 0;
    }
    unitlab_native_session_runtime_mark_report_health_stale(runtime, "missing-sequence");
    if (!expect_true(signal_state->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_STALE, "degraded report health did not stale signal")) {
        unitlab_native_session_manager_reset(&manager);
        return 0;
    }
    if (!expect_true(strcmp(signal_state->stale_reason, "missing-sequence") == 0, "stale reason not preserved")) {
        unitlab_native_session_manager_reset(&manager);
        return 0;
    }
    unitlab_native_session_runtime_mark_report_health_stale(runtime, "stale-generation");
    if (!expect_true(strcmp(signal_state->stale_reason, "missing-sequence") == 0, "stale-generation should not restale current cache")) {
        unitlab_native_session_manager_reset(&manager);
        return 0;
    }
    unitlab_native_session_manager_reset(&manager);
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

    {
        UnitLabNativeSignalUpdate signal_update;
        UnitLabNativeSignalChange signal_change;
        const UnitLabNativeSignalState* signal_state;

        memset(&signal_update, 0, sizeof(signal_update));
        snprintf(signal_update.signal_path, sizeof(signal_update.signal_path), "%s", "IED1LD0/XCBR1.Pos");
        snprintf(signal_update.data_reference, sizeof(signal_update.data_reference), "%s", "IED1LD0/XCBR1$ST$Pos$stVal");
        snprintf(signal_update.display_reference, sizeof(signal_update.display_reference), "%s", "IED1LD0/XCBR1.Pos.stVal");
        snprintf(signal_update.leaf_name, sizeof(signal_update.leaf_name), "%s", "stVal");
        snprintf(signal_update.value_summary, sizeof(signal_update.value_summary), "%s", "true");
        signal_update.leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE;
        signal_update.value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL;
        signal_update.bool_value = 1;
        signal_update.observed_at_ms = 5000U;
        signal_update.source_connection_generation = runtime->identity.connection_generation;

        signal_state = unitlab_native_signal_runtime_apply_update(&runtime->signal_runtime, &signal_update, &signal_change);
        if (!expect_true(signal_state != NULL && signal_state->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE, "signal did not become live")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
        unitlab_native_session_runtime_mark_closed(runtime);
        if (!expect_true(signal_state->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_STALE, "closed session did not mark signal stale")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
        if (!expect_true(signal_state->has_value == 1 && strcmp(signal_state->stale_reason, "session-closed") == 0, "stale signal lost provenance")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
    }

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
        UnitLabNativeSignalUpdate signal_update;
        UnitLabNativeSignalChange signal_change;
        const UnitLabNativeSignalState* signal_state;

        memset(&signal_update, 0, sizeof(signal_update));
        snprintf(signal_update.signal_path, sizeof(signal_update.signal_path), "%s", "IED1LD0/XCBR1.Pos");
        snprintf(signal_update.data_reference, sizeof(signal_update.data_reference), "%s", "IED1LD0/XCBR1$ST$Pos$stVal");
        snprintf(signal_update.display_reference, sizeof(signal_update.display_reference), "%s", "IED1LD0/XCBR1.Pos.stVal");
        snprintf(signal_update.leaf_name, sizeof(signal_update.leaf_name), "%s", "stVal");
        snprintf(signal_update.value_summary, sizeof(signal_update.value_summary), "%s", "false");
        signal_update.leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE;
        signal_update.value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL;
        signal_update.bool_value = 0;
        signal_update.observed_at_ms = 6000U;
        signal_update.source_connection_generation = runtime->identity.connection_generation;

        signal_state = unitlab_native_signal_runtime_apply_update(&runtime->signal_runtime, &signal_update, &signal_change);
        if (!expect_true(signal_state != NULL && signal_state->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE, "reconnected signal did not become live")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
        if (!expect_true(signal_state->bool_value == 0 && signal_state->stale_reason[0] == '\0', "reconnected signal did not replace stale value")) {
            unitlab_native_session_manager_reset(&manager);
            return 1;
        }
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
    if (!test_report_health_staleness_updates_live_signal_cache()) {
        return 1;
    }
    if (!test_report_sequence_policy_tracks_gaps_duplicates_and_generation_reset()) {
        return 1;
    }
    printf("native session runtime test passed\n");
    return 0;
}
