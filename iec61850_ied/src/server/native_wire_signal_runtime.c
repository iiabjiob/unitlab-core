#include "native_wire_signal_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char* destination, size_t destination_size, const char* source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    destination[0] = '\0';
    if (source != NULL && source[0] != '\0') {
        snprintf(destination, destination_size, "%s", source);
    }
}

static void copy_source_identity(UnitLabNativeSignalRuntime* runtime, const char* session_id, const char* endpoint_id, const char* device_key)
{
    if (runtime == NULL) {
        return;
    }
    copy_text(runtime->session_id, sizeof(runtime->session_id), session_id);
    copy_text(runtime->endpoint_id, sizeof(runtime->endpoint_id), endpoint_id);
    copy_text(runtime->device_key, sizeof(runtime->device_key), device_key);
}

static int ensure_capacity(UnitLabNativeSignalRuntime* runtime, size_t required)
{
    UnitLabNativeSignalState* resized;
    size_t new_capacity;

    if (runtime == NULL) {
        return 0;
    }
    if (required <= runtime->item_capacity) {
        return 1;
    }
    new_capacity = runtime->item_capacity != 0U ? runtime->item_capacity : 16U;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeSignalState*)realloc(runtime->items, new_capacity * sizeof(runtime->items[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > runtime->item_capacity) {
        memset(&resized[runtime->item_capacity], 0, (new_capacity - runtime->item_capacity) * sizeof(resized[0]));
    }
    runtime->items = resized;
    runtime->item_capacity = new_capacity;
    return 1;
}

static void zero_change(UnitLabNativeSignalChange* change)
{
    if (change != NULL) {
        memset(change, 0, sizeof(*change));
    }
}

static int same_text(const char* left, const char* right)
{
    if (left == NULL || left[0] == '\0') {
        return right == NULL || right[0] == '\0';
    }
    if (right == NULL || right[0] == '\0') {
        return 0;
    }
    return strcmp(left, right) == 0;
}

static size_t find_index(const UnitLabNativeSignalRuntime* runtime, const char* signal_path)
{
    if (runtime == NULL || signal_path == NULL || signal_path[0] == '\0') {
        return (size_t)-1;
    }
    for (size_t index = 0U; index < runtime->item_count; index++) {
        if (strcmp(runtime->items[index].signal_path, signal_path) == 0) {
            return index;
        }
    }
    return (size_t)-1;
}

void unitlab_native_signal_runtime_init(UnitLabNativeSignalRuntime* runtime)
{
    if (runtime == NULL) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
}

void unitlab_native_signal_runtime_reset(UnitLabNativeSignalRuntime* runtime)
{
    if (runtime == NULL) {
        return;
    }
    free(runtime->items);
    memset(runtime, 0, sizeof(*runtime));
}

void unitlab_native_signal_runtime_set_source_identity(
    UnitLabNativeSignalRuntime* runtime,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key)
{
    if (runtime == NULL) {
        return;
    }
    copy_source_identity(runtime, session_id, endpoint_id, device_key);
}

void unitlab_native_signal_runtime_set_observer(
    UnitLabNativeSignalRuntime* runtime,
    UnitLabNativeSignalObserver observer,
    void* user_data)
{
    if (runtime == NULL) {
        return;
    }
    runtime->observer = observer;
    runtime->observer_user_data = user_data;
}

const UnitLabNativeSignalState* unitlab_native_signal_runtime_find(
    const UnitLabNativeSignalRuntime* runtime,
    const char* signal_path)
{
    size_t index = find_index(runtime, signal_path);

    if (index == (size_t)-1) {
        return NULL;
    }
    return &runtime->items[index];
}

static void copy_signal_source(UnitLabNativeSignalState* state, const UnitLabNativeSignalRuntime* runtime, const UnitLabNativeSignalUpdate* update)
{
    copy_text(state->source_session_id, sizeof(state->source_session_id), runtime != NULL ? runtime->session_id : NULL);
    copy_text(state->source_endpoint_id, sizeof(state->source_endpoint_id), runtime != NULL ? runtime->endpoint_id : NULL);
    copy_text(state->source_device_key, sizeof(state->source_device_key), runtime != NULL ? runtime->device_key : NULL);
    state->source_connection_generation = update != NULL ? update->source_connection_generation : 0U;
    copy_text(state->source_report_rpt_id, sizeof(state->source_report_rpt_id), update != NULL ? update->source_report_rpt_id : NULL);
    copy_text(state->source_report_dat_set, sizeof(state->source_report_dat_set), update != NULL ? update->source_report_dat_set : NULL);
}

static void copy_signal_payload(UnitLabNativeSignalState* state, const UnitLabNativeSignalUpdate* update)
{
    if (state == NULL || update == NULL) {
        return;
    }
    copy_text(state->data_reference, sizeof(state->data_reference), update->data_reference);
    copy_text(state->display_reference, sizeof(state->display_reference), update->display_reference);
    copy_text(state->leaf_name, sizeof(state->leaf_name), update->leaf_name);
    copy_text(state->reason_labels, sizeof(state->reason_labels), update->reason_labels);
    state->observed_at_ms = update->observed_at_ms;
    state->leaf_role = update->leaf_role;
    state->reason_code = update->reason_code;
    if (update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE) {
        copy_text(state->value_summary, sizeof(state->value_summary), update->value_summary);
        state->value_kind = update->value_kind;
        state->unsigned_value = update->unsigned_value;
        state->integer_value = update->integer_value;
        state->floating_value = update->floating_value;
        state->bool_value = update->bool_value;
        state->has_value = 1;
    } else if (update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY) {
        copy_text(state->quality_summary, sizeof(state->quality_summary), update->quality_summary);
        state->quality_code = update->quality_code;
        copy_text(state->quality_validity, sizeof(state->quality_validity), update->quality_validity);
        state->has_quality = 1;
    } else if (update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP) {
        copy_text(state->timestamp_summary, sizeof(state->timestamp_summary), update->timestamp_summary);
        state->has_timestamp = 1;
    }
    state->update_count++;
}

static int update_changed(
    const UnitLabNativeSignalState* previous,
    const UnitLabNativeSignalRuntime* runtime,
    const UnitLabNativeSignalUpdate* update)
{
    if (previous == NULL || update == NULL) {
        return 0;
    }
    if (!same_text(previous->data_reference, update->data_reference)
        || !same_text(previous->display_reference, update->display_reference)
        || !same_text(previous->leaf_name, update->leaf_name)) {
        return 1;
    }
    if (!same_text(previous->source_session_id, runtime != NULL ? runtime->session_id : NULL)
        || !same_text(previous->source_endpoint_id, runtime != NULL ? runtime->endpoint_id : NULL)
        || !same_text(previous->source_device_key, runtime != NULL ? runtime->device_key : NULL)
        || previous->source_connection_generation != update->source_connection_generation
        || !same_text(previous->source_report_rpt_id, update->source_report_rpt_id)
        || !same_text(previous->source_report_dat_set, update->source_report_dat_set)) {
        return 1;
    }
    if (previous->leaf_role != update->leaf_role || previous->reason_code != update->reason_code || !same_text(previous->reason_labels, update->reason_labels)) {
        return 1;
    }
    if (update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE) {
        if (previous->value_kind != update->value_kind
            || previous->unsigned_value != update->unsigned_value
            || previous->integer_value != update->integer_value
            || previous->floating_value != update->floating_value
            || previous->bool_value != update->bool_value
            || !same_text(previous->value_summary, update->value_summary)) {
            return 1;
        }
    } else if (update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY) {
        if (previous->quality_code != update->quality_code
            || !same_text(previous->quality_validity, update->quality_validity)
            || !same_text(previous->quality_summary, update->quality_summary)) {
            return 1;
        }
    } else if (update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP) {
        if (!same_text(previous->timestamp_summary, update->timestamp_summary)) {
            return 1;
        }
    }
    return 0;
}

static int signal_value_changed(const UnitLabNativeSignalState* previous, const UnitLabNativeSignalUpdate* update)
{
    if (update == NULL || update->leaf_role != UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE) {
        return 0;
    }
    if (previous == NULL || previous->has_value == 0) {
        return 1;
    }
    return previous->value_kind != update->value_kind
        || previous->unsigned_value != update->unsigned_value
        || previous->integer_value != update->integer_value
        || previous->floating_value != update->floating_value
        || previous->bool_value != update->bool_value
        || !same_text(previous->value_summary, update->value_summary);
}

static int signal_quality_changed(const UnitLabNativeSignalState* previous, const UnitLabNativeSignalUpdate* update)
{
    if (update == NULL || update->leaf_role != UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY) {
        return 0;
    }
    if (previous == NULL || previous->has_quality == 0) {
        return 1;
    }
    return previous->quality_code != update->quality_code
        || !same_text(previous->quality_validity, update->quality_validity)
        || !same_text(previous->quality_summary, update->quality_summary);
}

static int signal_timestamp_changed(const UnitLabNativeSignalState* previous, const UnitLabNativeSignalUpdate* update)
{
    if (update == NULL || update->leaf_role != UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP) {
        return 0;
    }
    if (previous == NULL || previous->has_timestamp == 0) {
        return 1;
    }
    return !same_text(previous->timestamp_summary, update->timestamp_summary);
}

static void fill_change_flags(
    const UnitLabNativeSignalState* previous,
    const UnitLabNativeSignalUpdate* update,
    int is_new,
    UnitLabNativeSignalChange* change)
{
    if (change == NULL || update == NULL) {
        return;
    }
    change->is_new = is_new;
    change->value_changed = signal_value_changed(previous, update) || (is_new && update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE);
    change->quality_changed = signal_quality_changed(previous, update) || (is_new && update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY);
    change->timestamp_changed = signal_timestamp_changed(previous, update) || (is_new && update->leaf_role == UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP);
}

UnitLabNativeSignalState* unitlab_native_signal_runtime_apply_update(
    UnitLabNativeSignalRuntime* runtime,
    const UnitLabNativeSignalUpdate* update,
    UnitLabNativeSignalChange* change)
{
    UnitLabNativeSignalState* state = NULL;
    UnitLabNativeSignalState previous;
    UnitLabNativeSignalChange local_change;
    UnitLabNativeSignalChange* emitted_change = change;
    size_t index;
    int changed;
    int is_new = 0;

    if (emitted_change == NULL) {
        emitted_change = &local_change;
    }
    zero_change(emitted_change);
    if (runtime == NULL || update == NULL || update->signal_path[0] == '\0') {
        return NULL;
    }
    memset(&previous, 0, sizeof(previous));
    index = find_index(runtime, update->signal_path);
    if (index == (size_t)-1) {
        if (!ensure_capacity(runtime, runtime->item_count + 1U)) {
            return NULL;
        }
        state = &runtime->items[runtime->item_count];
        memset(state, 0, sizeof(*state));
        copy_text(state->signal_path, sizeof(state->signal_path), update->signal_path);
        runtime->item_count++;
        is_new = 1;
    } else {
        state = &runtime->items[index];
        previous = *state;
    }
    changed = is_new ? 1 : update_changed(&previous, runtime, update);
    copy_signal_source(state, runtime, update);
    copy_signal_payload(state, update);
    runtime->update_count++;
    if (changed) {
        runtime->change_count++;
        state->last_changed_ms = update->observed_at_ms;
        state->version++;
        fill_change_flags(is_new ? NULL : &previous, update, is_new, emitted_change);
        if (runtime->observer != NULL) {
            runtime->observer(state, emitted_change, runtime->observer_user_data);
        }
    }
    return state;
}
