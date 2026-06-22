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
    int last_freshness_changed;
    int last_became_stale;
    int last_became_live;
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
    state->last_freshness_changed = change->freshness_changed;
    state->last_became_stale = change->became_stale;
    state->last_became_live = change->became_live;
}

static void set_text(char* destination, size_t destination_size, const char* value)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    snprintf(destination, destination_size, "%s", value != NULL ? value : "");
}

static void populate_value_update(
    UnitLabNativeSignalUpdate* update,
    const char* signal_path,
    const char* data_reference,
    const char* display_reference,
    const char* leaf_name,
    const char* value_summary,
    int bool_value,
    uint64_t observed_at_ms,
    uint64_t generation)
{
    memset(update, 0, sizeof(*update));
    set_text(update->signal_path, sizeof(update->signal_path), signal_path);
    set_text(update->data_reference, sizeof(update->data_reference), data_reference);
    set_text(update->display_reference, sizeof(update->display_reference), display_reference);
    set_text(update->leaf_name, sizeof(update->leaf_name), leaf_name);
    set_text(update->value_summary, sizeof(update->value_summary), value_summary);
    update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE;
    update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL;
    update->bool_value = bool_value;
    update->observed_at_ms = observed_at_ms;
    update->source_connection_generation = generation;
}

static void populate_quality_update(
    UnitLabNativeSignalUpdate* update,
    const char* signal_path,
    const char* data_reference,
    const char* display_reference,
    const char* leaf_name,
    const char* quality_summary,
    const char* quality_validity,
    uint32_t quality_code,
    uint64_t observed_at_ms,
    uint64_t generation)
{
    memset(update, 0, sizeof(*update));
    set_text(update->signal_path, sizeof(update->signal_path), signal_path);
    set_text(update->data_reference, sizeof(update->data_reference), data_reference);
    set_text(update->display_reference, sizeof(update->display_reference), display_reference);
    set_text(update->leaf_name, sizeof(update->leaf_name), leaf_name);
    set_text(update->quality_summary, sizeof(update->quality_summary), quality_summary);
    set_text(update->quality_validity, sizeof(update->quality_validity), quality_validity);
    update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY;
    update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BIT_STRING;
    update->quality_code = quality_code;
    update->observed_at_ms = observed_at_ms;
    update->source_connection_generation = generation;
}

static void populate_timestamp_update(
    UnitLabNativeSignalUpdate* update,
    const char* signal_path,
    const char* data_reference,
    const char* display_reference,
    const char* leaf_name,
    const char* timestamp_summary,
    uint64_t observed_at_ms,
    uint64_t generation)
{
    memset(update, 0, sizeof(*update));
    set_text(update->signal_path, sizeof(update->signal_path), signal_path);
    set_text(update->data_reference, sizeof(update->data_reference), data_reference);
    set_text(update->display_reference, sizeof(update->display_reference), display_reference);
    set_text(update->leaf_name, sizeof(update->leaf_name), leaf_name);
    set_text(update->timestamp_summary, sizeof(update->timestamp_summary), timestamp_summary);
    update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP;
    update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_STRING;
    update->observed_at_ms = observed_at_ms;
    update->source_connection_generation = generation;
}

static void assert_change_flags(
    const UnitLabNativeSignalChange* change,
    int is_new,
    int value_changed,
    int quality_changed,
    int timestamp_changed,
    int freshness_changed,
    int became_stale,
    int became_live)
{
    assert(change != NULL);
    assert(change->is_new == is_new);
    assert(change->value_changed == value_changed);
    assert(change->quality_changed == quality_changed);
    assert(change->timestamp_changed == timestamp_changed);
    assert(change->freshness_changed == freshness_changed);
    assert(change->became_stale == became_stale);
    assert(change->became_live == became_live);
}

static void test_signal_freshness_and_observer_notifications(void)
{
    UnitLabNativeSignalRuntime runtime;
    UnitLabNativeSignalUpdate update;
    UnitLabNativeSignalChange change;
    SignalObserverState observer_state;
    const UnitLabNativeSignalState* signal;
    uint64_t initial_version;
    uint64_t initial_last_changed;

    memset(&observer_state, 0, sizeof(observer_state));
    unitlab_native_signal_runtime_init(&runtime);
    unitlab_native_signal_runtime_set_source_identity(&runtime, "session-a", "mms:IED1@127.0.0.1:102", "IED1");
    unitlab_native_signal_runtime_set_current_connection_generation(&runtime, 1U);
    unitlab_native_signal_runtime_set_observer(&runtime, signal_observer, &observer_state);

    populate_value_update(
        &update,
        "IED1LD0/XCBR1.Pos",
        "IED1LD0/XCBR1$ST$Pos$stVal",
        "IED1LD0/XCBR1.Pos.stVal",
        "stVal",
        "true",
        1,
        1000U,
        1U);

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.item_count == 1U);
    assert(runtime.update_count == 1U);
    assert(runtime.change_count == 1U);
    assert(observer_state.call_count == 1U);
    assert(strcmp(signal->signal_path, "IED1LD0/XCBR1.Pos") == 0);
    assert(strcmp(signal->source_session_id, "session-a") == 0);
    assert(strcmp(signal->source_endpoint_id, "mms:IED1@127.0.0.1:102") == 0);
    assert(strcmp(signal->source_device_key, "IED1") == 0);
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert(signal->bool_value == 1);
    assert(signal->has_value == 1);
    assert(signal->observed_at_ms == 1000U);
    assert(signal->last_changed_ms == 1000U);
    assert(signal->version == 1U);
    assert_change_flags(&change, 1, 1, 0, 0, 1, 0, 1);
    initial_version = signal->version;
    initial_last_changed = signal->last_changed_ms;

    populate_value_update(
        &update,
        "IED1LD0/XCBR1.Pos",
        "IED1LD0/XCBR1$ST$Pos$stVal",
        "IED1LD0/XCBR1.Pos.stVal",
        "stVal",
        "true",
        1,
        2000U,
        1U);

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.item_count == 1U);
    assert(runtime.update_count == 2U);
    assert(runtime.change_count == 1U);
    assert(observer_state.call_count == 1U);
    assert(signal->observed_at_ms == 2000U);
    assert(signal->last_changed_ms == initial_last_changed);
    assert(signal->version == initial_version);
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert_change_flags(&change, 0, 0, 0, 0, 0, 0, 0);

    populate_quality_update(
        &update,
        "IED1LD0/XCBR1.Pos",
        "IED1LD0/XCBR1$ST$Pos$q",
        "IED1LD0/XCBR1.Pos.q",
        "q",
        "0x0001",
        "questionable",
        0x0001U,
        2500U,
        1U);

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.update_count == 3U);
    assert(runtime.change_count == 2U);
    assert(observer_state.call_count == 2U);
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert(signal->quality_code == 0x0001U);
    assert(strcmp(signal->quality_validity, "questionable") == 0);
    assert(signal->has_quality == 1);
    assert_change_flags(&change, 0, 0, 1, 0, 0, 0, 0);

    populate_timestamp_update(
        &update,
        "IED1LD0/XCBR1.Pos",
        "IED1LD0/XCBR1$ST$Pos$t",
        "IED1LD0/XCBR1.Pos.t",
        "t",
        "2026-06-22T00:00:00Z",
        2600U,
        1U);

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.update_count == 4U);
    assert(runtime.change_count == 3U);
    assert(observer_state.call_count == 3U);
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert(signal->has_timestamp == 1);
    assert(strcmp(signal->timestamp_summary, "2026-06-22T00:00:00Z") == 0);
    assert_change_flags(&change, 0, 0, 0, 1, 0, 0, 0);

    {
        size_t stale_count = unitlab_native_signal_runtime_mark_source_stale(&runtime, "session-a", 0U, "transport-lost", 3000U);
        assert(stale_count == 1U);
    }
    assert(runtime.change_count == 4U);
    assert(observer_state.call_count == 4U);
    signal = unitlab_native_signal_runtime_find(&runtime, "IED1LD0/XCBR1.Pos");
    assert(signal != NULL);
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_STALE);
    assert(strcmp(signal->stale_reason, "transport-lost") == 0);
    assert(signal->stale_at_ms == 3000U);
    assert(signal->stale_generation == 1U);
    assert(signal->bool_value == 1);
    assert(signal->quality_code == 0x0001U);
    assert(signal->has_quality == 1);
    assert(signal->has_timestamp == 1);
    assert(strcmp(signal->timestamp_summary, "2026-06-22T00:00:00Z") == 0);
    assert(signal->last_changed_ms == 3000U);
    assert(observer_state.last_freshness_changed == 1);
    assert(observer_state.last_became_stale == 1);
    assert(observer_state.last_became_live == 0);

    {
        size_t stale_count = unitlab_native_signal_runtime_mark_source_stale(&runtime, "session-a", 0U, "transport-lost", 3000U);
        assert(stale_count == 0U);
    }
    assert(runtime.change_count == 4U);
    assert(observer_state.call_count == 4U);

    populate_value_update(
        &update,
        "IED1LD0/XCBR1.Pos",
        "IED1LD0/XCBR1$ST$Pos$stVal",
        "IED1LD0/XCBR1.Pos.stVal",
        "stVal",
        "false",
        0,
        4000U,
        1U);

    signal = unitlab_native_signal_runtime_apply_update(&runtime, &update, &change);
    assert(signal != NULL);
    assert(runtime.update_count == 5U);
    assert(runtime.change_count == 5U);
    assert(observer_state.call_count == 5U);
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert(signal->bool_value == 0);
    assert(signal->quality_code == 0x0001U);
    assert(signal->has_quality == 1);
    assert(signal->has_timestamp == 1);
    assert(strcmp(signal->timestamp_summary, "2026-06-22T00:00:00Z") == 0);
    assert(signal->stale_reason[0] == '\0');
    assert(signal->stale_at_ms == 0U);
    assert(signal->stale_generation == 0U);
    assert(signal->version == initial_version + 4U);
    assert_change_flags(&change, 0, 1, 0, 0, 1, 0, 1);

    unitlab_native_signal_runtime_reset(&runtime);
}

static void test_signal_generation_guard_and_cache_key(void)
{
    UnitLabNativeSignalRuntime runtime;
    UnitLabNativeSignalUpdate update;
    const UnitLabNativeSignalState* first;
    const UnitLabNativeSignalState* second;

    unitlab_native_signal_runtime_init(&runtime);
    unitlab_native_signal_runtime_set_source_identity(&runtime, "session-b", "mms:IED1@127.0.0.1:102", "IED1");
    unitlab_native_signal_runtime_set_current_connection_generation(&runtime, 1U);

    populate_value_update(
        &update,
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "IED1LD0/MMXU1$MX$PhV$phsA$cVal$mag$f",
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "f",
        "1",
        1,
        500U,
        1U);
    assert(unitlab_native_signal_runtime_apply_update(&runtime, &update, NULL) != NULL);

    populate_value_update(
        &update,
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "IED1LD0/MMXU1$MX$PhV$phsA$cVal$mag$f$alt",
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f.alt",
        "f",
        "1",
        1,
        600U,
        1U);
    assert(unitlab_native_signal_runtime_apply_update(&runtime, &update, NULL) != NULL);
    assert(runtime.item_count == 1U);

    populate_value_update(
        &update,
        "IED1LD0/MMXU1.PhV.phsB.cVal.mag.f",
        "IED1LD0/MMXU1$MX$PhV$phsB$cVal$mag$f",
        "IED1LD0/MMXU1.PhV.phsB.cVal.mag.f",
        "f",
        "2",
        1,
        700U,
        1U);
    assert(unitlab_native_signal_runtime_apply_update(&runtime, &update, NULL) != NULL);

    first = unitlab_native_signal_runtime_find(&runtime, "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f");
    second = unitlab_native_signal_runtime_find(&runtime, "IED1LD0/MMXU1.PhV.phsB.cVal.mag.f");
    assert(first != NULL);
    assert(second != NULL);
    assert(first != second);
    assert(runtime.item_count == 2U);
    assert(strcmp(first->data_reference, "IED1LD0/MMXU1$MX$PhV$phsA$cVal$mag$f$alt") == 0);
    assert(strcmp(second->signal_path, "IED1LD0/MMXU1.PhV.phsB.cVal.mag.f") == 0);

    unitlab_native_signal_runtime_set_current_connection_generation(&runtime, 2U);
    {
        size_t stale_count = unitlab_native_signal_runtime_mark_source_stale(&runtime, "session-b", 0U, "reconnecting", 800U);
        assert(stale_count == 2U);
    }
    assert(runtime.change_count == 5U);
    assert(runtime.stale_generation_drop_count == 0U);
    assert(first->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_STALE);
    assert(second->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_STALE);

    populate_value_update(
        &update,
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "IED1LD0/MMXU1$MX$PhV$phsA$cVal$mag$f",
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "f",
        "3",
        1,
        900U,
        1U);
    assert(unitlab_native_signal_runtime_apply_update(&runtime, &update, NULL) == first);
    assert(runtime.stale_generation_drop_count == 1U);
    assert(first->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_STALE);
    assert(strcmp(first->value_summary, "1") == 0);

    populate_value_update(
        &update,
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "IED1LD0/MMXU1$MX$PhV$phsA$cVal$mag$f",
        "IED1LD0/MMXU1.PhV.phsA.cVal.mag.f",
        "f",
        "3",
        1,
        1000U,
        2U);
    assert(unitlab_native_signal_runtime_apply_update(&runtime, &update, NULL) == first);
    assert(first->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert(first->source_connection_generation == 2U);
    assert(strcmp(first->stale_reason, "") == 0);
    assert(first->stale_at_ms == 0U);

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
    unitlab_native_session_runtime_set_identity(&runtime, "session-c", "mms:IED1@127.0.0.1:102", "IED1");
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
    assert(signal->freshness == UNITLAB_NATIVE_SIGNAL_FRESHNESS_LIVE);
    assert(signal->has_value == 1);
    assert(signal->has_quality == 1);
    assert(signal->has_timestamp == 1);
    assert(strcmp(signal->source_session_id, "session-c") == 0);
    assert(signal->observed_at_ms == 4242U);
    assert(signal->update_count >= 3U);

    unitlab_native_session_runtime_mark_closed(&runtime);
}

int main(void)
{
    test_signal_freshness_and_observer_notifications();
    test_signal_generation_guard_and_cache_key();
    test_runtime_adapts_report_entries_to_one_signal();
    return 0;
}
