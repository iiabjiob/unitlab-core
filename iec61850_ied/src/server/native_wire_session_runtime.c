#include "native_wire_session_runtime.h"

#include "native_wire_client_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t session_runtime_now_ms(void)
{
    time_t now = time(NULL);
    if (now <= (time_t)0) {
        return 0U;
    }
    return (uint64_t)now * 1000U;
}

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

static int ensure_manager_capacity(UnitLabNativeSessionManager* manager, size_t required)
{
    UnitLabNativeSessionRuntime* resized;
    size_t new_capacity;

    if (manager == NULL) {
        return 0;
    }
    if (required <= manager->item_capacity) {
        return 1;
    }
    new_capacity = manager->item_capacity != 0U ? manager->item_capacity : 1U;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeSessionRuntime*)realloc(manager->items, new_capacity * sizeof(manager->items[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > manager->item_capacity) {
        memset(&resized[manager->item_capacity], 0, (new_capacity - manager->item_capacity) * sizeof(resized[0]));
    }
    manager->items = resized;
    manager->item_capacity = new_capacity;
    return 1;
}

static int same_identity(const UnitLabNativeSessionRuntime* runtime, const char* session_id, const char* endpoint_id, const char* device_key)
{
    if (runtime == NULL) {
        return 0;
    }
    if (session_id != NULL && session_id[0] != '\0' && strcmp(runtime->identity.session_id, session_id) != 0) {
        return 0;
    }
    if (endpoint_id != NULL && endpoint_id[0] != '\0' && strcmp(runtime->identity.endpoint_id, endpoint_id) != 0) {
        return 0;
    }
    if (device_key != NULL && device_key[0] != '\0' && strcmp(runtime->identity.device_key, device_key) != 0) {
        return 0;
    }
    return runtime->identity.session_id[0] != '\0' || runtime->identity.endpoint_id[0] != '\0' || runtime->identity.device_key[0] != '\0';
}

static void log_runtime_event(
    const UnitLabNativeSessionRuntime* runtime,
    const char* event,
    const char* detail)
{
    if (runtime == NULL || event == NULL || event[0] == '\0') {
        return;
    }
    printf(
        "native-session-runtime: event=%s session_id=%s endpoint_id=%s device_key=%s generation=%llu phase=%s detail=%s\n",
        event,
        runtime->identity.session_id[0] != '\0' ? runtime->identity.session_id : "<none>",
        runtime->identity.endpoint_id[0] != '\0' ? runtime->identity.endpoint_id : "<none>",
        runtime->identity.device_key[0] != '\0' ? runtime->identity.device_key : "<none>",
        (unsigned long long)runtime->identity.connection_generation,
        unitlab_native_session_phase_label(runtime->live.phase),
        detail != NULL && detail[0] != '\0' ? detail : "<none>");
    fflush(stdout);
}

static void runtime_set_error(UnitLabNativeSessionRuntime* runtime, const char* error_code, const char* error_message)
{
    if (runtime == NULL) {
        return;
    }
    copy_text(runtime->live.last_error_code, sizeof(runtime->live.last_error_code), error_code != NULL && error_code[0] != '\0' ? error_code : "SESSION_RUNTIME_OK");
    copy_text(runtime->live.last_error_message, sizeof(runtime->live.last_error_message), error_message != NULL && error_message[0] != '\0' ? error_message : "");
}

static void runtime_mark_signals_stale(
    UnitLabNativeSessionRuntime* runtime,
    const char* reason,
    uint64_t stale_at_ms,
    uint64_t connection_generation)
{
    if (runtime == NULL) {
        return;
    }
    (void)unitlab_native_signal_runtime_mark_source_stale(
        &runtime->signal_runtime,
        runtime->identity.session_id,
        connection_generation,
        reason,
        stale_at_ms);
}

static int runtime_has_active_desired_state(const UnitLabNativeSessionRuntime* runtime)
{
    if (runtime == NULL) {
        return 0;
    }
    return runtime->desired.endpoint_connected || runtime->desired.discovery_available || runtime->desired.subscription_active || runtime->desired.reporting_active;
}

static int operation_in_flight(const UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind);

void unitlab_native_session_runtime_init(UnitLabNativeSessionRuntime* runtime)
{
    if (runtime == NULL) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    unitlab_native_signal_runtime_init(&runtime->signal_runtime);
    runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_IDLE;
    runtime_set_error(runtime, "SESSION_RUNTIME_OK", NULL);
}

void unitlab_native_session_manager_init(UnitLabNativeSessionManager* manager)
{
    if (manager == NULL) {
        return;
    }
    memset(manager, 0, sizeof(*manager));
}

void unitlab_native_session_manager_reset(UnitLabNativeSessionManager* manager)
{
    if (manager == NULL) {
        return;
    }
    free(manager->worker_owners);
    free(manager->items);
    memset(manager, 0, sizeof(*manager));
}

void unitlab_native_session_runtime_set_identity(
    UnitLabNativeSessionRuntime* runtime,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key)
{
    if (runtime == NULL) {
        return;
    }
    copy_text(runtime->identity.session_id, sizeof(runtime->identity.session_id), session_id);
    copy_text(runtime->identity.endpoint_id, sizeof(runtime->identity.endpoint_id), endpoint_id);
    copy_text(runtime->identity.device_key, sizeof(runtime->identity.device_key), device_key);
    if (runtime->identity.session_id[0] == '\0') {
        copy_text(runtime->identity.session_id, sizeof(runtime->identity.session_id), runtime->identity.endpoint_id);
    }
    if (runtime->identity.device_key[0] == '\0') {
        copy_text(runtime->identity.device_key, sizeof(runtime->identity.device_key), runtime->identity.endpoint_id);
    }
    unitlab_native_signal_runtime_set_source_identity(
        &runtime->signal_runtime,
        runtime->identity.session_id,
        runtime->identity.endpoint_id,
        runtime->identity.device_key);
    unitlab_native_signal_runtime_set_current_connection_generation(&runtime->signal_runtime, runtime->identity.connection_generation);
    log_runtime_event(runtime, "session-configured", NULL);
}

void unitlab_native_session_runtime_set_desired_state(
    UnitLabNativeSessionRuntime* runtime,
    int endpoint_connected,
    int discovery_available,
    int subscription_active,
    int reporting_active)
{
    if (runtime == NULL) {
        return;
    }
    runtime->desired.endpoint_connected = endpoint_connected ? 1 : 0;
    runtime->desired.discovery_available = discovery_available ? 1 : 0;
    runtime->desired.subscription_active = subscription_active ? 1 : 0;
    runtime->desired.reporting_active = reporting_active ? 1 : 0;
    log_runtime_event(runtime, "desired-state-updated", NULL);
}

void unitlab_native_session_runtime_set_subscription_intent(
    UnitLabNativeSessionRuntime* runtime,
    const char* rcb_key,
    int wants_subscription,
    int wants_gi)
{
    if (runtime == NULL) {
        return;
    }
    copy_text(runtime->identity.rcb_key, sizeof(runtime->identity.rcb_key), rcb_key);
    runtime->intent.wants_subscription = wants_subscription ? 1 : 0;
    runtime->intent.wants_gi = wants_gi ? 1 : 0;
    runtime->desired.endpoint_connected = 1;
    runtime->desired.discovery_available = 1;
    runtime->desired.subscription_active = runtime->intent.wants_subscription;
    runtime->desired.reporting_active = runtime->intent.wants_subscription || runtime->intent.wants_gi;
    log_runtime_event(runtime, "subscription-intent", runtime->identity.rcb_key[0] != '\0' ? runtime->identity.rcb_key : NULL);
}

UnitLabNativeSessionRuntime* unitlab_native_session_manager_get_or_create(
    UnitLabNativeSessionManager* manager,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key)
{
    UnitLabNativeSessionRuntime* runtime = NULL;

    if (manager == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < manager->item_count; index++) {
        if (same_identity(&manager->items[index], session_id, endpoint_id, device_key)) {
            return &manager->items[index];
        }
    }
    if (!ensure_manager_capacity(manager, manager->item_count + 1U)) {
        return NULL;
    }
    runtime = &manager->items[manager->item_count];
    unitlab_native_session_runtime_init(runtime);
    unitlab_native_session_runtime_set_identity(runtime, session_id, endpoint_id, device_key);
    manager->item_count++;
    log_runtime_event(runtime, "session-created", NULL);
    return runtime;
}

const char* unitlab_native_session_phase_label(UnitLabNativeSessionPhase phase)
{
    switch (phase) {
    case UNITLAB_NATIVE_SESSION_PHASE_IDLE:
        return "idle";
    case UNITLAB_NATIVE_SESSION_PHASE_CONNECTING:
        return "connecting";
    case UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED:
        return "associated";
    case UNITLAB_NATIVE_SESSION_PHASE_DISCOVERING:
        return "discovering";
    case UNITLAB_NATIVE_SESSION_PHASE_DISCOVERED:
        return "discovered";
    case UNITLAB_NATIVE_SESSION_PHASE_SUBSCRIBING:
        return "subscribing";
    case UNITLAB_NATIVE_SESSION_PHASE_REPORTING:
        return "reporting";
    case UNITLAB_NATIVE_SESSION_PHASE_RECONNECTING:
        return "reconnecting";
    case UNITLAB_NATIVE_SESSION_PHASE_DEGRADED:
        return "degraded";
    case UNITLAB_NATIVE_SESSION_PHASE_FAILED:
        return "failed";
    case UNITLAB_NATIVE_SESSION_PHASE_CLOSED:
        return "closed";
    }
    return "unknown";
}

int unitlab_native_session_runtime_next_desired_operation(
    const UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind* operation_kind)
{
    if (operation_kind == NULL) {
        return 0;
    }
    *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_CONNECT;
    if (runtime == NULL) {
        return 0;
    }
    if (runtime_has_active_desired_state(runtime) && (runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_FAILED || runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_DEGRADED || runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_CLOSED)) {
        *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT;
        return 1;
    }
    if (!runtime->live.associated && runtime->identity.connection_generation > 0U && runtime_has_active_desired_state(runtime)) {
        *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT;
        return 1;
    }
    if (runtime->desired.endpoint_connected && !runtime->live.associated) {
        *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_CONNECT;
        return 1;
    }
    if (runtime->desired.discovery_available && !runtime->live.discovered) {
        *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER;
        return 1;
    }
    if (runtime->desired.subscription_active && (!runtime->live.subscribed || !runtime->live.reporting)) {
        *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE;
        return 1;
    }
    if (runtime->desired.reporting_active && !runtime->live.reporting) {
        *operation_kind = UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE;
        return 1;
    }
    return 0;
}

int unitlab_native_session_runtime_operation_is_in_flight(
    const UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind operation_kind)
{
    return operation_in_flight(runtime, operation_kind);
}

static int operation_in_flight(const UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind)
{
    if (runtime == NULL) {
        return 0;
    }
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        return 0;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        return runtime->live.connect_in_flight;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        return runtime->live.discover_in_flight;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        return runtime->live.subscribe_in_flight;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        return runtime->live.reconnect_in_flight;
    }
    return 0;
}

static int* operation_in_flight_slot(UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind)
{
    if (runtime == NULL) {
        return NULL;
    }
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        return NULL;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        return &runtime->live.connect_in_flight;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        return &runtime->live.discover_in_flight;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        return &runtime->live.subscribe_in_flight;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        return &runtime->live.reconnect_in_flight;
    }
    return NULL;
}

static void update_phase_for_begin(UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind)
{
    if (runtime == NULL) {
        return;
    }
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_CONNECTING;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_DISCOVERING;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_SUBSCRIBING;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_RECONNECTING;
        break;
    }
}

int unitlab_native_session_runtime_begin_operation(
    UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind operation_kind)
{
    int* inflight_slot;
    const char* event_name = NULL;

    if (runtime == NULL) {
        return 0;
    }
    if (operation_in_flight(runtime, operation_kind)) {
        log_runtime_event(runtime, "duplicate-operation-collapsed", unitlab_native_session_phase_label(runtime->live.phase));
        return 1;
    }
    inflight_slot = operation_in_flight_slot(runtime, operation_kind);
    if (inflight_slot == NULL) {
        return 0;
    }
    if (operation_kind == UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT) {
        runtime_mark_signals_stale(runtime, "reconnecting", session_runtime_now_ms(), 0U);
        runtime->identity.connection_generation++;
        unitlab_native_signal_runtime_set_current_connection_generation(&runtime->signal_runtime, runtime->identity.connection_generation);
    } else if (runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_CLOSED || runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_FAILED) {
        runtime->identity.connection_generation++;
        unitlab_native_signal_runtime_set_current_connection_generation(&runtime->signal_runtime, runtime->identity.connection_generation);
    } else if (operation_kind == UNITLAB_NATIVE_SESSION_OPERATION_CONNECT && !runtime->live.associated) {
        runtime->identity.connection_generation++;
        unitlab_native_signal_runtime_set_current_connection_generation(&runtime->signal_runtime, runtime->identity.connection_generation);
    }
    *inflight_slot = 1;
    update_phase_for_begin(runtime, operation_kind);
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        event_name = "operation-none";
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        event_name = "connect-started";
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        event_name = "discover-started";
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        event_name = "subscribe-started";
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        event_name = "reconnect-started";
        break;
    }
    log_runtime_event(runtime, event_name, NULL);
    return 1;
}

static void clear_operation_in_flight(UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind)
{
    int* inflight_slot = operation_in_flight_slot(runtime, operation_kind);
    if (inflight_slot != NULL) {
        *inflight_slot = 0;
    }
}

static void mark_operation_success(UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind)
{
    if (runtime == NULL) {
        return;
    }
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        runtime->live.associated = 1;
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED;
        runtime->live.discovered = 0;
        runtime->live.subscribed = 0;
        runtime->live.reporting = 0;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        runtime->live.discovered = 1;
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_DISCOVERED;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        runtime->live.subscribed = 1;
        runtime->live.reporting = 1;
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_REPORTING;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        runtime->live.associated = 1;
        runtime->live.discovered = 0;
        runtime->live.subscribed = 0;
        runtime->live.reporting = 0;
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED;
        break;
    }
}

static void mark_operation_failure(UnitLabNativeSessionRuntime* runtime, UnitLabNativeSessionOperationKind operation_kind, int degraded, const char* error_code, const char* error_message)
{
    if (runtime == NULL) {
        return;
    }
    runtime_mark_signals_stale(runtime, error_message != NULL && error_message[0] != '\0' ? error_message : error_code, session_runtime_now_ms(), 0U);
    runtime_set_error(runtime, error_code, error_message);
    if (degraded) {
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_DEGRADED;
    } else {
        runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_FAILED;
    }
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        runtime->live.associated = 0;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        runtime->live.discovered = 0;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        runtime->live.subscribed = 0;
        runtime->live.reporting = 0;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        runtime->live.associated = 0;
        runtime->live.discovered = 0;
        runtime->live.subscribed = 0;
        runtime->live.reporting = 0;
        break;
    }
}

void unitlab_native_session_runtime_complete_operation(
    UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionOperationKind operation_kind,
    int success,
    const char* error_code,
    const char* error_message)
{
    if (runtime == NULL) {
        return;
    }
    clear_operation_in_flight(runtime, operation_kind);
    if (success) {
        mark_operation_success(runtime, operation_kind);
        runtime_set_error(runtime, "SESSION_RUNTIME_OK", NULL);
        log_runtime_event(runtime, "operation-completed", unitlab_native_session_phase_label(runtime->live.phase));
        return;
    }
    mark_operation_failure(
        runtime,
        operation_kind,
        operation_kind == UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER || operation_kind == UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE || operation_kind == UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT,
        error_code,
        error_message);
    log_runtime_event(runtime, "operation-failed", runtime->live.last_error_message);
}

void unitlab_native_session_runtime_mark_report_received(
    UnitLabNativeSessionRuntime* runtime,
    uint64_t timestamp_ms)
{
    if (runtime == NULL) {
        return;
    }
    runtime->live.reporting = 1;
    if (runtime->live.subscribed == 0) {
        runtime->live.subscribed = 1;
    }
    runtime->live.last_report_timestamp_ms = timestamp_ms != 0U ? timestamp_ms : session_runtime_now_ms();
    runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_REPORTING;
    log_runtime_event(runtime, "report-received", NULL);
}

static void build_signal_update(
    const UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeClientSessionState* session,
    const UnitLabNativeLastReportEntry* entry,
    uint64_t timestamp_ms,
    UnitLabNativeSignalUpdate* update)
{
    char reference[384U];
    char* last_dot;

    if (update == NULL) {
        return;
    }
    memset(update, 0, sizeof(*update));
    if (entry == NULL) {
        return;
    }
    if (entry->display_reference[0] != '\0') {
        snprintf(reference, sizeof(reference), "%s", entry->display_reference);
    } else {
        snprintf(reference, sizeof(reference), "%s", entry->data_reference);
    }
    for (size_t index = 0U; reference[index] != '\0'; index++) {
        if (reference[index] == '$') {
            reference[index] = '.';
        }
    }
    last_dot = strrchr(reference, '.');
    if (last_dot != NULL && last_dot[1] != '\0') {
        snprintf(update->leaf_name, sizeof(update->leaf_name), "%s", last_dot + 1);
        *last_dot = '\0';
    }
    snprintf(update->data_reference, sizeof(update->data_reference), "%s", entry->data_reference);
    snprintf(update->display_reference, sizeof(update->display_reference), "%s", entry->display_reference);
    snprintf(update->signal_path, sizeof(update->signal_path), "%s", reference);
    snprintf(update->value_summary, sizeof(update->value_summary), "%s", entry->value_summary);
    snprintf(update->quality_summary, sizeof(update->quality_summary), "%s", entry->quality_validity);
    snprintf(update->reason_labels, sizeof(update->reason_labels), "%s", entry->reason_labels);
    snprintf(update->source_report_rpt_id, sizeof(update->source_report_rpt_id), "%s", session != NULL ? session->discovered_model.last_report_rpt_id : "");
    snprintf(update->source_report_dat_set, sizeof(update->source_report_dat_set), "%s", session != NULL ? session->discovered_model.last_report_data_set : "");
    update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_OTHER;
    if (strcmp(update->leaf_name, "q") == 0) {
        update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_QUALITY;
    } else if (strcmp(update->leaf_name, "t") == 0) {
        update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_TIMESTAMP;
    } else {
        update->leaf_role = UNITLAB_NATIVE_SIGNAL_LEAF_ROLE_VALUE;
    }
    switch (entry->value_kind) {
    case UNITLAB_NATIVE_REPORT_VALUE_BOOL:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BOOL;
        update->bool_value = entry->bool_value;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_UNSIGNED:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_UNSIGNED;
        update->unsigned_value = entry->unsigned_value;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_INTEGER:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_INTEGER;
        update->integer_value = entry->integer_value;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_FLOAT:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_FLOAT;
        update->floating_value = entry->floating_value;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_STRING:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_STRING;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_OCTETS:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_OCTETS;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_BIT_STRING:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_BIT_STRING;
        update->quality_code = entry->quality_code;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_STRUCTURE:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_STRUCTURE;
        break;
    case UNITLAB_NATIVE_REPORT_VALUE_EMPTY:
    case UNITLAB_NATIVE_REPORT_VALUE_UNSUPPORTED:
    default:
        update->value_kind = UNITLAB_NATIVE_SIGNAL_VALUE_KIND_UNKNOWN;
        break;
    }
    update->quality_code = entry->quality_code;
    snprintf(update->quality_validity, sizeof(update->quality_validity), "%s", entry->quality_validity);
    update->reason_code = entry->reason_code;
    update->observed_at_ms = timestamp_ms;
    update->source_connection_generation = runtime != NULL ? runtime->identity.connection_generation : 0U;
}

void unitlab_native_session_runtime_apply_last_report_to_signals(
    UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeClientSessionState* session,
    uint64_t timestamp_ms)
{
    if (runtime == NULL || session == NULL) {
        return;
    }
    unitlab_native_signal_runtime_set_source_identity(
        &runtime->signal_runtime,
        runtime->identity.session_id,
        runtime->identity.endpoint_id,
        runtime->identity.device_key);
    unitlab_native_signal_runtime_set_current_connection_generation(&runtime->signal_runtime, runtime->identity.connection_generation);
    for (size_t index = 0U; index < session->last_report_entry_count; index++) {
        UnitLabNativeSignalUpdate update;
        UnitLabNativeSignalChange change;

        build_signal_update(runtime, session, &session->last_report_entries[index], timestamp_ms, &update);
        (void)unitlab_native_signal_runtime_apply_update(&runtime->signal_runtime, &update, &change);
    }
}

void unitlab_native_session_runtime_update_discovery_snapshot(
    UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeDiscoverySnapshot* snapshot)
{
    if (runtime == NULL || snapshot == NULL) {
        return;
    }
    runtime->discovery_snapshot = *snapshot;
    runtime->has_discovery_snapshot = 1;
    runtime->live.discovered = 1;
    runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_DISCOVERED;
    log_runtime_event(runtime, "discovery-snapshot-updated", runtime->discovery_snapshot.snapshot_id);
}

void unitlab_native_session_runtime_mark_degraded(
    UnitLabNativeSessionRuntime* runtime,
    const char* error_code,
    const char* error_message)
{
    if (runtime == NULL) {
        return;
    }
    runtime_mark_signals_stale(runtime, error_message != NULL && error_message[0] != '\0' ? error_message : error_code, session_runtime_now_ms(), 0U);
    runtime_set_error(runtime, error_code, error_message);
    runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_DEGRADED;
    log_runtime_event(runtime, "session-degraded", runtime->live.last_error_message);
}

void unitlab_native_session_runtime_mark_failed(
    UnitLabNativeSessionRuntime* runtime,
    const char* error_code,
    const char* error_message)
{
    if (runtime == NULL) {
        return;
    }
    runtime_mark_signals_stale(runtime, error_message != NULL && error_message[0] != '\0' ? error_message : error_code, session_runtime_now_ms(), 0U);
    runtime_set_error(runtime, error_code, error_message);
    runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_FAILED;
    log_runtime_event(runtime, "session-failed", runtime->live.last_error_message);
}

void unitlab_native_session_runtime_mark_closed(UnitLabNativeSessionRuntime* runtime)
{
    if (runtime == NULL) {
        return;
    }
    runtime_mark_signals_stale(runtime, "session-closed", session_runtime_now_ms(), 0U);
    runtime->live.connect_in_flight = 0;
    runtime->live.discover_in_flight = 0;
    runtime->live.subscribe_in_flight = 0;
    runtime->live.reconnect_in_flight = 0;
    runtime->live.associated = 0;
    runtime->live.discovered = 0;
    runtime->live.subscribed = 0;
    runtime->live.reporting = 0;
    runtime->live.phase = UNITLAB_NATIVE_SESSION_PHASE_CLOSED;
    runtime_set_error(runtime, "SESSION_RUNTIME_OK", NULL);
    log_runtime_event(runtime, "session-closed", NULL);
}

int unitlab_native_session_runtime_copy_status(
    const UnitLabNativeSessionRuntime* runtime,
    UnitLabNativeSessionStatus* status)
{
    if (runtime == NULL || status == NULL) {
        return 0;
    }
    memset(status, 0, sizeof(*status));
    copy_text(status->session_id, sizeof(status->session_id), runtime->identity.session_id);
    copy_text(status->endpoint_id, sizeof(status->endpoint_id), runtime->identity.endpoint_id);
    copy_text(status->device_key, sizeof(status->device_key), runtime->identity.device_key);
    copy_text(status->phase, sizeof(status->phase), unitlab_native_session_phase_label(runtime->live.phase));
    copy_text(status->last_error_code, sizeof(status->last_error_code), runtime->live.last_error_code);
    copy_text(status->last_error_message, sizeof(status->last_error_message), runtime->live.last_error_message);
    status->associated = runtime->live.associated;
    status->discovered = runtime->live.discovered;
    status->subscribed = runtime->live.subscribed;
    status->reporting = runtime->live.reporting;
    status->desired_endpoint_connected = runtime->desired.endpoint_connected;
    status->desired_discovery_available = runtime->desired.discovery_available;
    status->desired_subscription_active = runtime->desired.subscription_active;
    status->desired_reporting_active = runtime->desired.reporting_active;
    status->wants_subscription = runtime->intent.wants_subscription;
    status->wants_gi = runtime->intent.wants_gi;
    status->connection_generation = runtime->identity.connection_generation;
    status->last_report_timestamp_ms = runtime->live.last_report_timestamp_ms;
    status->has_discovery_snapshot = runtime->has_discovery_snapshot;
    if (runtime->has_discovery_snapshot) {
        copy_text(status->snapshot_id, sizeof(status->snapshot_id), runtime->discovery_snapshot.snapshot_id);
        status->logical_device_count = runtime->discovery_snapshot.logical_device_count;
        status->logical_node_count = runtime->discovery_snapshot.logical_node_count;
        status->data_set_count = runtime->discovery_snapshot.data_set_count;
        status->data_set_member_count = runtime->discovery_snapshot.data_set_member_count;
        status->report_control_count = runtime->discovery_snapshot.report_control_count;
        status->signal_count = runtime->discovery_snapshot.signal_count;
    }
    return 1;
}
