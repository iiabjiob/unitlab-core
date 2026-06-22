#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_WORKER_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_WORKER_H

/* Internal session worker: owns the reconciliation loop for a single MMS session. */

#include "native_wire_session_runtime.h"

typedef int (*UnitLabNativeSessionWorkerOperationHandler)(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);

typedef struct {
    UnitLabNativeSessionWorkerOperationHandler connect;
    UnitLabNativeSessionWorkerOperationHandler discover;
    UnitLabNativeSessionWorkerOperationHandler subscribe;
    UnitLabNativeSessionWorkerOperationHandler reconnect;
} UnitLabNativeSessionWorkerHandlers;

typedef struct UnitLabNativeSessionWorker {
    UnitLabNativeSessionManager* manager;
    UnitLabNativeSessionRuntime* runtime;
    UnitLabNativeSessionWorkerHandlers handlers;
    void* user_data;
    int running;
    int stop_requested;
    uint64_t reconcile_count;
} UnitLabNativeSessionWorker;

void unitlab_native_session_worker_init(
    UnitLabNativeSessionWorker* worker,
    UnitLabNativeSessionManager* manager,
    UnitLabNativeSessionRuntime* runtime,
    const UnitLabNativeSessionWorkerHandlers* handlers,
    void* user_data);
int unitlab_native_session_manager_open_worker(
    UnitLabNativeSessionManager* manager,
    UnitLabNativeSessionWorker* worker,
    const char* session_id,
    const char* endpoint_id,
    const char* device_key,
    const UnitLabNativeSessionWorkerHandlers* handlers,
    void* user_data);
int unitlab_native_session_worker_start(UnitLabNativeSessionWorker* worker);
void unitlab_native_session_worker_stop(UnitLabNativeSessionWorker* worker);
int unitlab_native_session_worker_reconcile_once(UnitLabNativeSessionWorker* worker);
size_t unitlab_native_session_worker_run_until_idle(UnitLabNativeSessionWorker* worker, size_t max_steps);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_WORKER_H */
