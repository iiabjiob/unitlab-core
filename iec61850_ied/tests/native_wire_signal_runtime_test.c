#include "server/native_wire_signal_runtime.h"
#include "server/native_wire_session_runtime.h"
#include "server/native_wire_client_session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    size_t call_count;
    char last_path[384U];
    int last_is_new;
    int last_value_changed;
    int last_quality_changed;
    int last_timestamp_changed;
} SignalObserverState;

static void signal_observer(
    const UnitLabNativeSignalState* signal,
    const UnitLabNativeSignalChange* change,
    void* user_data)
{
    SignalObserverState* state = (SignalObserverState*)user_data;

    if (state == NULL || signal == NULL || change == NULL) {
        return;
    }
    state->call_count++;
    snprintf(state->last_path, sizeof(state->last_path), "%s", signal->signal_path);
    state->last_is_new = change->is_new;
    state->last_value_changed = change->value_changed;
    state->last_quality_changed = change->quality_changed;
    state->last_timestamp_changed = change->timestamp_changed;
}

static void test_runtime_cache_and_notifications(void)
{
    UnitLabNativeSignalRuntime runtime;
    UnitLabNativeSignalUpdate update;
    UnitLabNativeSignalChange change;
    SignalObserverState observer_state;
    const UnitLabNativeSignalState* signal;

    memset(&observer_state, 0, sizeof(observer_state));
    unitlab_native_signal_runtime_init(&runtime);
    unitlab_native_signal_runtime_set_source_identity(&runtime, "session-a", "mms:IED1@127.0.0.1:102", "IED1");
    unitlab_native_signal_runtime_set_observer(&runtime, signal_observer, &observer_state);

    memset(&update, 0, sizeof(update));
    snprintf(update.signal_path, sizeof(update.signal_path), "%s", "IED1LD0/XCBR1.Pos");
    snprintf(update.data_reference, sizeof(update.data_reference), "%s", "IED1LD0/XCBR1$ST$Pos$stVal");
    snprintf(update.display_reference, sizeof(update.display_reference), "%s", "IED1LD0/XCBR1.Pos.stVal");
    snprintf(update.leaf_name, sizeof(update.leaf_name), "%s", "stVal");
    snprintf(update.value_summary, sizeof(update.value_summary), "%s", "true");
    update.leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE;
    update.value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL;
    update.bool_value = 1;
    update.observed_at_ms = 1000U;
    update.source_connection_generation = 3U;

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.item_count == 1U);
    assert(runtime.update_count == 1U);
    assert(runtime.change_count == 1U);
    assert(observer_state.call_count == 1U);
    assert(strcmp(signal->source_session_id, "session-a") == 0);
    assert(strcmp(signal->source_endpoint_id, "mms:IED1@127.0.0.1:102") == 0);
    assert(signal->bool_value == 1);
    assert(signal->has_value == 1);
    assert(change.is_new == 1);
    assert(change.value_changed == 1);

    memset(&update, 0, sizeof(update));
    snprintf(update.signal_path, sizeof(update.signal_path), "%s", "IED1LD0/XCBR1.Pos");
    snprintf(update.data_reference, sizeof(update.data_reference), "%s", "IED1LD0/XCBR1$ST$Pos$q");
    snprintf(update.display_reference, sizeof(update.display_reference), "%s", "IED1LD0/XCBR1.Pos.q");
    snprintf(update.leaf_name, sizeof(update.leaf_name), "%s", "q");
    snprintf(update.quality_summary, sizeof(update.quality_summary), "%s", "0x0000");
    snprintf(update.quality_validity, sizeof(update.quality_validity), "%s", "good");
    update.leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY;
    update.value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BIT_STRING;
    update.quality_code = 0x0000U;
    update.observed_at_ms = 1001U;
    update.source_connection_generation = 3U;

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.item_count == 1U);
    assert(runtime.update_count == 2U);
    assert(runtime.change_count == 2U);
    assert(observer_state.call_count == 2U);
    assert(signal->bool_value == 1);
    assert(signal->quality_code == 0x0000U);
    assert(strcmp(signal->quality_validity, "good") == 0);
    assert(signal->has_quality == 1);
    assert(change.value_changed == 0);
    assert(change.quality_changed == 1);

    memset(&update, 0, sizeof(update));
    snprintf(update.signal_path, sizeof(update.signal_path), "%s", "IED1LD0/XCBR1.Pos");
    snprintf(update.data_reference, sizeof(update.data_reference), "%s", "IED1LD0/XCBR1$ST$Pos$t");
    snprintf(update.display_reference, sizeof(update.display_reference), "%s", "IED1LD0/XCBR1.Pos.t");
    snprintf(update.leaf_name, sizeof(update.leaf_name), "%s", "t");
    snprintf(update.timestamp_summary, sizeof(update.timestamp_summary), "%s", "2026-06-22T00:00:00Z");
    update.leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP;
    update.value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_STRING;
    update.observed_at_ms = 1002U;
    update.source_connection_generation = 3U;

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.item_count == 1U);
    assert(runtime.update_count == 3U);
    assert(runtime.change_count == 3U);
    assert(observer_state.call_count == 3U);
    assert(signal->has_timestamp == 1);
    assert(strcmp(signal->timestamp_summary, "2026-06-22T00:00:00Z") == 0);
    assert(change.timestamp_changed == 1);

    unitlab_native_signal_runtime_reset(&runtime);
}

static void test_runtime_adapts_report_entries_to_one_signal(void)
{
    UnitLabNativeSessionRuntime runtime;
    UnitLabNativeClientSessionState session;
    UnitLabNativeDiscoveredLeafRef* leaf_ref;
    UnitLabNativeLastReportEntry* entry;
    const UnitLabNativeSignalState* signal;

    unitlab_native_session_runtime_init(&runtime);
    unitlab_native_session_runtime_set_identity(&runtime, "session-b", "mms:IED1@127.0.0.1:102", "IED1");
    memset(&session, 0, sizeof(session));
    leaf_ref = unitlab_native_client_session_append_leaf_ref(&session, "IED1LD0/XCBR1$ST$Pos$stVal");
    assert(leaf_ref != NULL);

    entry = unitlab_native_client_session_append_last_report_entry(&session, "IED1LD0/XCBR1$ST$Pos$stVal", 1, 0U);
    assert(entry != NULL);
    snprintf(entry->value_summary, sizeof(entry->value_summary), "%s", "true");
    entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_BOOL;
    entry->bool_value = 1;

    entry = unitlab_native_client_session_append_last_report_entry(&session, "IED1LD0/XCBR1$ST$Pos$q", 1, 1U);
    assert(entry != NULL);
    snprintf(entry->quality_validity, sizeof(entry->quality_validity), "%s", "good");
    entry->quality_code = 0x0000U;

    entry = unitlab_native_client_session_append_last_report_entry(&session, "IED1LD0/XCBR1$ST$Pos$t", 1, 2U);
    assert(entry != NULL);
    snprintf(entry->value_summary, sizeof(entry->value_summary), "%s", "2026-06-22T00:00:00Z");
    entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_STRING;

    unitlab_native_session_runtime_apply_last_report_to_signals(&runtime, &session, 4242U);

    assert(runtime.signal_runtime.item_count == 1U);
    signal = unitlab_native_signal_runtime_find(&runtime.signal_runtime, runtime.signal_runtime.items[0].signal_path);
    assert(signal != NULL);
    assert(signal->has_value == 1);
    assert(signal->has_quality == 1);
    assert(signal->has_timestamp == 1);
    assert(strcmp(signal->source_session_id, "session-b") == 0);
    assert(signal->observed_at_ms == 4242U);
    assert(signal->update_count >= 3U);

    unitlab_native_session_runtime_mark_closed(&runtime);
}

int main(void)
{
    test_runtime_cache_and_notifications();
    test_runtime_adapts_report_entries_to_one_signal();
    return 0;
}
