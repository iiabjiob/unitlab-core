#include "native_wire_session_worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t worker_now_epoch_ms(void)
{
    time_t now = time(NULL);
    if (now <= (time_t)0) {
        return 0U;
    }
    return (uint64_t)now * 1000U;
}

static const char* operation_label(UnitLabNativeSessionOperationKind operation_kind)
{
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        return "none";
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        return "connect";
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        return "discover";
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        return "subscribe";
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        return "reconnect";
    }
    return "unknown";
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

static size_t manager_runtime_index(const UnitLabNativeSessionManager* manager, const UnitLabNativeSessionRuntime* runtime)
{
    size_t index;

    if (manager == NULL || runtime == NULL) {
        return (size_t)-1;
    }
    for (index = 0U; index < manager->item_count; index++) {
        if (&manager->items[index] == runtime) {
            return index;
        }
    }
    return (size_t)-1;
}

static int ensure_worker_owner_capacity(UnitLabNativeSessionManager* manager, size_t required)
{
    UnitLabNativeSessionWorker** resized;
    size_t new_capacity;

    if (manager == NULL) {
        return 0;
    }
    if (required <= manager->worker_owner_capacity) {
        return 1;
    }
    new_capacity = manager->worker_owner_capacity != 0U ? manager->worker_owner_capacity : 1U;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeSessionWorker**)realloc(manager->worker_owners, new_capacity * sizeof(manager->worker_owners[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > manager->worker_owner_capacity) {
        memset(&resized[manager->worker_owner_capacity], 0, (new_capacity - manager->worker_owner_capacity) * sizeof(resized[0]));
    }
    manager->worker_owners = resized;
    manager->worker_owner_capacity = new_capacity;
    return 1;
}

static int manager_claim_worker(UnitLabNativeSessionWorker* worker)
{
    size_t runtime_index;
    UnitLabNativeSessionWorker* existing;

    if (worker == NULL || worker->manager == NULL || worker->runtime == NULL) {
        return 0;
    }
    runtime_index = manager_runtime_index(worker->manager, worker->runtime);
    if (runtime_index == (size_t)-1) {
        return 0;
    }
    if (!ensure_worker_owner_capacity(worker->manager, worker->manager->item_count)) {
        return 0;
    }
    existing = worker->manager->worker_owners[runtime_index];
    if (existing != NULL && existing != worker) {
        return 0;
    }
    worker->manager->worker_owners[runtime_index] = worker;
    return 1;
}

static void manager_release_worker(UnitLabNativeSessionWorker* worker)
{
    size_t runtime_index;

    if (worker == NULL || worker->manager == NULL || worker->runtime == NULL || worker->manager->worker_owners == NULL) {
        return;
    }
    runtime_index = manager_runtime_index(worker->manager, worker->runtime);
    if (runtime_index == (size_t)-1 || runtime_index >= worker->manager->worker_owner_capacity) {
        return;
    }
    if (worker->manager->worker_owners[runtime_index] == worker) {
        worker->manager->worker_owners[runtime_index] = NULL;
    }
}

static void worker_log(const UnitLabNativeSessionWorker* worker, const char* event, const char* detail, UnitLabNativeSessionOperationKind operation_kind)
{
    if (worker == NULL || worker->runtime == NULL || event == NULL || event[0] == '\0') {
        return;
    }
    printf(
        "native-session-worker: event=%s session_id=%s endpoint_id=%s generation=%llu phase=%s operation=%s detail=%s\n",
        event,
        worker->runtime->identity.session_id[0] != '\0' ? worker->runtime->identity.session_id : "<none>",
        worker->runtime->identity.endpoint_id[0] != '\0' ? worker->runtime->identity.endpoint_id : "<none>",
        (unsigned long long)worker->runtime->identity.connection_generation,
        unitlab_native_session_phase_label(worker->runtime->live.phase),
        operation_label(operation_kind),
        detail != NULL && detail[0] != '\0' ? detail : "<none>");
    fflush(stdout);
}

static void worker_set_default_error(char* error_code, size_t error_code_size, char* error_message, size_t error_message_size, const char* code, const char* message)
{
    copy_text(error_code, error_code_size, code);
    copy_text(error_message, error_message_size, message);
}

static int invoke_handler(
    const UnitLabNativeSessionWorker* worker,
    UnitLabNativeSessionOperationKind operation_kind,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    UnitLabNativeSessionWorkerOperationHandler handler = NULL;

    if (worker == NULL) {
        worker_set_default_error(error_code, error_code_size, error_message, error_message_size, "SESSION_WORKER_INVALID", "Worker is not initialized.");
        return 0;
    }
    switch (operation_kind) {
    case UNITLAB_NATIVE_SESSION_OPERATION_NONE:
        handler = NULL;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_CONNECT:
        handler = worker->handlers.connect;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER:
        handler = worker->handlers.discover;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_SUBSCRIBE:
        handler = worker->handlers.subscribe;
        break;
    case UNITLAB_NATIVE_SESSION_OPERATION_RECONNECT:
        handler = worker->handlers.reconnect;
        break;
    }
    if (handler == NULL) {
        worker_set_default_error(error_code, error_code_size, error_message, error_message_size, "SESSION_WORKER_NO_HANDLER", "No handler was installed for the selected operation.");
        return 0;
    }
    return handler(worker->user_data, worker->runtime, error_code, error_code_size, error_message, error_message_size) ? 1 : 0;
}

void unitlab_native_session_worker_init(
    UnitLabNativeSessionWorker* worker,
    UnitLabNativeSessionManager* manager,
    UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeSessionWorkerHandlers* handlers,
    void* user_data)
{
    if (worker == NULL) {
        return;
    }
    memset(worker, 0, sizeof(*worker));
    worker->manager = manager;
    worker->runtime = runtime;
    worker->user_data = user_data;
    if (handlers != NULL) {
        worker->handlers = *handlers;
    }
}

int unitlab_native_session_manager_open_worker(
    UnitLabNativeSessionManager* manager,
    UnitLabNativeSessionWorker* worker,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key,
    const UnitLabNativeSessionWorkerHandlers* handlers,
    void* user_data)
{
    UnitLabNativeSessionRuntime* runtime;

    if (manager == NULL || worker == NULL) {
        return 0;
    }
    runtime = unitlab_native_session_manager_get_or_create(manager, session_id, endpoint_id, device_key);
    if (runtime == NULL) {
        return 0;
    }
    unitlab_native_session_worker_init(worker, manager, runtime, handlers, user_data);
    return 1;
}

int unitlab_native_session_worker_start(UnitLabNativeSessionWorker* worker)
{
    if (worker == NULL || worker->runtime == NULL) {
        return 0;
    }
    if (worker->running) {
        worker_log(worker, "worker-start-collapsed", NULL, UNITLAB_NATIVE_SESSION_OPERATION_NONE);
        return 1;
    }
    if (!manager_claim_worker(worker)) {
        worker_log(worker, "worker-owner-conflict", NULL, UNITLAB_NATIVE_SESSION_OPERATION_NONE);
        return 0;
    }
    worker->stop_requested = 0;
    worker->running = 1;
    worker_log(worker, "worker-started", NULL, UNITLAB_NATIVE_SESSION_OPERATION_NONE);
    return 1;
}

void unitlab_native_session_worker_stop(UnitLabNativeSessionWorker* worker)
{
    if (worker == NULL) {
        return;
    }
    worker->stop_requested = 1;
    if (worker->runtime != NULL) {
        unitlab_native_session_runtime_mark_closed(worker->runtime);
    }
    worker->running = 0;
    manager_release_worker(worker);
    worker_log(worker, "worker-stopped", NULL, UNITLAB_NATIVE_SESSION_OPERATION_NONE);
}

int unitlab_native_session_worker_reconcile_once(UnitLabNativeSessionWorker* worker)
{
    UnitLabNativeSessionOperationKind operation_kind;
    char error_code[64U];
    char error_message[256U];
    int success;

    if (worker == NULL || worker->runtime == NULL || !worker->running || worker->stop_requested) {
        return 0;
    }
    if (!unitlab_native_session_runtime_next_desired_operation(worker->runtime, &operation_kind)) {
        return 0;
    }
    worker_log(worker, "reconcile-selected-operation", NULL, operation_kind);
    if (unitlab_native_session_runtime_operation_is_in_flight(worker->runtime, operation_kind)) {
        worker_log(worker, "duplicate-operation-suppressed", NULL, operation_kind);
        return 0;
    }
    if (!unitlab_native_session_runtime_begin_operation(worker->runtime, operation_kind)) {
        worker_log(worker, "reconcile-begin-failed", NULL, operation_kind);
        return 0;
    }
    copy_text(error_code, sizeof(error_code), "SESSION_RUNTIME_OK");
    error_message[0] = '\0';
    success = invoke_handler(worker, operation_kind, error_code, sizeof(error_code), error_message, sizeof(error_message));
    unitlab_native_session_runtime_complete_operation(
        worker->runtime,
        operation_kind,
        success,
        success ? NULL : error_code,
        success ? NULL : error_message);
    worker_log(worker, success ? "operation-completed" : "operation-failed", success ? unitlab_native_session_phase_label(worker->runtime->live.phase) : worker->runtime->live.last_error_message, operation_kind);
    if (success && operation_kind == UNITLAB_NATIVE_SESSION_OPERATION_DISCOVER) {
        worker->runtime->discovery_snapshot.created_at_ms = worker_now_epoch_ms();
    }
    worker->reconcile_count++;
    return 1;
}

size_t unitlab_native_session_worker_run_until_idle(UnitLabNativeSessionWorker* worker, size_t max_steps)
{
    size_t steps = 0U;
    UnitLabNativeSessionOperationKind pending_operation = UNITLAB_NATIVE_SESSION_OPERATION_NONE;

    if (worker == NULL) {
        return 0U;
    }
    while (steps < max_steps && unitlab_native_session_worker_reconcile_once(worker)) {
        steps++;
    }
    if (steps == max_steps && worker->runtime != NULL && unitlab_native_session_runtime_next_desired_operation(worker->runtime, &pending_operation)) {
        worker_log(worker, "reconcile-step-limit-reached", NULL, UNITLAB_NATIVE_SESSION_OPERATION_NONE);
    }
    return steps;
}
