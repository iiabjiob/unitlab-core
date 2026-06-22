#ifndef UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_BACKEND_H
#define UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_BACKEND_H

/* Internal MMS session backend: owns the concrete native client transport context used by SessionWorker. */

#include "server_runtime.h"
#include "native_wire_client_session.h"
#include "native_wire_session_runtime.h"
#include "native_wire_session_worker.h"

typedef struct UnitLabNativeWireClientWorkerContext {
    const UnitLabIedServerConfig* config;
    UnitLabNativeClientSessionState session;
    int data_fd;
    int control_fd;
} UnitLabNativeWireClientWorkerContext;

void unitlab_native_wire_client_worker_context_init(
    UnitLabNativeWireClientWorkerContext* context,
    const UnitLabIedServerConfig* config);
void unitlab_native_wire_client_worker_context_reset(
    UnitLabNativeWireClientWorkerContext* context);

int unitlab_native_wire_client_worker_connect(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);
int unitlab_native_wire_client_worker_discover(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);
int unitlab_native_wire_client_worker_subscribe(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);
int unitlab_native_wire_client_worker_reconnect(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);

void unitlab_native_wire_client_worker_default_handlers(UnitLabNativeSessionWorkerHandlers* handlers);

#endif /* UNITLAB_IEC61850_IED_NATIVE_WIRE_SESSION_BACKEND_H */
