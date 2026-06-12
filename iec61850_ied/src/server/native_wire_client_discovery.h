#ifndef UNITLAB_NATIVE_WIRE_CLIENT_DISCOVERY_H
#define UNITLAB_NATIVE_WIRE_CLIENT_DISCOVERY_H

#include <stddef.h>
#include <stdint.h>

#include "native_wire_client_session.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

typedef struct UnitLabNativeDiscoveryIo UnitLabNativeDiscoveryIo;

typedef int (*UnitLabNativeDiscoveryGetNameListStep)(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* continue_after,
    uint32_t invoke_id);

typedef int (*UnitLabNativeDiscoveryReadStep)(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id);

typedef int (*UnitLabNativeDiscoveryAttributesStep)(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list);

typedef void (*UnitLabNativeDiscoveryModelSummary)(
    const UnitLabNativeClientSessionState* session,
    const char* phase);

struct UnitLabNativeDiscoveryIo {
    int data_fd;
    uint8_t* scratch;
    size_t scratch_length;
    uint8_t* request;
    size_t request_length;
    uint8_t* response;
    size_t response_length;
    size_t* encoded_response_length;
    uint8_t* text_buffer;
    size_t text_buffer_length;
    UnitLabMmsDiagnostic* diagnostic;
    UnitLabNativeDiscoveryGetNameListStep get_name_list_step;
    UnitLabNativeDiscoveryReadStep read_step;
    UnitLabNativeDiscoveryAttributesStep attributes_step;
    UnitLabNativeDiscoveryModelSummary emit_model_summary;
};

int unitlab_native_client_run_discover_sequence(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* domain_id,
    uint32_t invoke_id,
    uint32_t* next_invoke_id);

#endif
