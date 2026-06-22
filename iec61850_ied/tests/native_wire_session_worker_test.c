#include "server/native_wire_session_worker.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int connect_calls;
    int discover_calls;
    int subscribe_calls;
    int reconnect_calls;
} WorkerTestContext;

static int expect_true(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

static int connect_handler(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    WorkerTestContext* context = (WorkerTestContext*)user_data;

    (void)runtime;
    if (context != NULL) {
        context->connect_calls++;
    }
    if (error_code != NULL && error_code_size > 0U) {
        snprintf(error_code, error_code_size, "%s", "SESSION_RUNTIME_OK");
    }
    if (error_message != NULL && error_message_size > 0U) {
        error_message[0] = '\0';
    }
    return 1;
}

static int discover_handler(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    WorkerTestContext* context = (WorkerTestContext*)user_data;
    UnitLabNativeDiscoverySnapshot snapshot;

    if (context != NULL) {
        context->discover_calls++;
    }
    memset(&snapshot, 0, sizeof(snapshot));
    snprintf(snapshot.snapshot_id, sizeof(snapshot.snapshot_id), "%s", "snapshot-worker");
    snprintf(snapshot.endpoint_id, sizeof(snapshot.endpoint_id), "%s", runtime != NULL ? runtime->identity.endpoint_id : "<none>");
    snprintf(snapshot.device_key, sizeof(snapshot.device_key), "%s", runtime != NULL ? runtime->identity.device_key : "<none>");
    snapshot.logical_device_count = 1U;
    snapshot.logical_node_count = 2U;
    snapshot.data_set_count = 3U;
    snapshot.data_set_member_count = 4U;
    snapshot.report_control_count = 5U;
    snapshot.signal_count = 6U;
    unitlab_native_session_runtime_update_discovery_snapshot(runtime, &snapshot);
    if (error_code != NULL && error_code_size > 0U) {
        snprintf(error_code, error_code_size, "%s", "SESSION_RUNTIME_OK");
    }
    if (error_message != NULL && error_message_size > 0U) {
        error_message[0] = '\0';
    }
    return 1;
}

static int subscribe_handler(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    WorkerTestContext* context = (WorkerTestContext*)user_data;

    (void)runtime;
    if (context != NULL) {
        context->subscribe_calls++;
    }
    if (error_code != NULL && error_code_size > 0U) {
        snprintf(error_code, error_code_size, "%s", "SESSION_RUNTIME_OK");
    }
    if (error_message != NULL && error_message_size > 0U) {
        error_message[0] = '\0';
    }
    return 1;
}

static int reconnect_handler(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    WorkerTestContext* context = (WorkerTestContext*)user_data;

    (void)runtime;
    if (context != NULL) {
        context->reconnect_calls++;
    }
    if (error_code != NULL && error_code_size > 0U) {
        snprintf(error_code, error_code_size, "%s", "SESSION_RUNTIME_OK");
    }
    if (error_message != NULL && error_message_size > 0U) {
        error_message[0] = '\0';
    }
    return 1;
}

int main(void)
{
    UnitLabNativeSessionManager manager;
    UnitLabNativeSessionWorker worker;
    UnitLabNativeSessionWorker second_worker;
    UnitLabNativeSessionWorkerHandlers handlers;
    WorkerTestContext context;
    UnitLabNativeSessionOperationKind next_operation;
    size_t steps;

    memset(&context, 0, sizeof(context));
    memset(&second_worker, 0, sizeof(second_worker));
    memset(&handlers, 0, sizeof(handlers));
    handlers.connect = connect_handler;
    handlers.discover = discover_handler;
    handlers.subscribe = subscribe_handler;
    handlers.reconnect = reconnect_handler;

    unitlab_native_session_manager_init(&manager);
    if (!expect_true(unitlab_native_session_manager_open_worker(&manager, &worker, "session-worker", "mms:IED1@127.0.0.1:102", "IED1", &handlers, &context), "worker open failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    unitlab_native_session_runtime_set_desired_state(worker.runtime, 1, 1, 1, 1);
    unitlab_native_session_runtime_set_subscription_intent(worker.runtime, "IED1LD0/LLN0.brcbA", 1, 1);

    if (!expect_true(unitlab_native_session_worker_start(&worker), "worker start failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(unitlab_native_session_worker_start(&worker), "duplicate worker start should collapse")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    steps = unitlab_native_session_worker_run_until_idle(&worker, 8U);
    if (!expect_true(steps == 3U, "worker did not reconcile connect/discover/subscribe")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(context.connect_calls == 1 && context.discover_calls == 1 && context.subscribe_calls == 1 && context.reconnect_calls == 0, "unexpected initial worker handler counts")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(unitlab_native_session_runtime_next_desired_operation(worker.runtime, &next_operation) == 0, "worker should be idle after reporting")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(worker.runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_REPORTING, "worker did not reach reporting")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    if (!expect_true(unitlab_native_session_manager_open_worker(&manager, &second_worker, "session-worker", "mms:IED1@127.0.0.1:102", "IED1", &handlers, &context), "second worker open failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(!unitlab_native_session_worker_start(&second_worker), "duplicate worker owner should be rejected")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    unitlab_native_session_worker_stop(&worker);
    if (!expect_true(worker.runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_CLOSED, "worker stop did not close runtime")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(worker.running == 0, "worker still running after stop")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    if (!expect_true(unitlab_native_session_worker_start(&second_worker), "worker restart after release failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(unitlab_native_session_worker_reconcile_once(&second_worker), "worker reconnect reconcile failed")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(context.reconnect_calls == 1, "worker did not select reconnect after close")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }
    if (!expect_true(second_worker.runtime->live.phase == UNITLAB_NATIVE_SESSION_PHASE_ASSOCIATED, "reconnect did not restore associated phase")) {
        unitlab_native_session_manager_reset(&manager);
        return 1;
    }

    unitlab_native_session_worker_stop(&second_worker);
    unitlab_native_session_manager_reset(&manager);
    printf("native session worker test passed\n");
    return 0;
}
