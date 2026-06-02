#include "unitlab_mms_server_runtime.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "protocols/mms/unitlab_mms_runtime_bridge.h"

static void server_runtime_set_diagnostic(UnitLabMmsDiagnostic* diagnostic, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (diagnostic == NULL) {
        return;
    }
    diagnostic->code = code;
    if (message == NULL) {
        diagnostic->message[0] = '\0';
        return;
    }
    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
}

static int server_runtime_validate_config(const UnitLabIedServerConfig* config, UnitLabMmsDiagnostic* diagnostic)
{
    if (config == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server config is required.");
        return 0;
    }
    if (config->bind_address == NULL || config->bind_address[0] == '\0') {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server bind address is required.");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server port must be in range 1..65535.");
        return 0;
    }
    return 1;
}

static void server_runtime_fail_and_capture(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsOperationResult* operation_result)
{
    if (server_runtime == NULL || operation_result == NULL) {
        return;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
}

static int server_runtime_encode_ber_element(
    UnitLabMmsBerTagClass tag_class,
    int constructed,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;

    unitlab_mms_ber_element_init(&element);
    element.tag.tag_class = tag_class;
    element.tag.constructed = constructed;
    element.tag.tag_number = tag_number;
    element.value_bytes = value_bytes;
    element.value_length = value_length;
    return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
}

static int server_runtime_encode_invoke_id_element(
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t invoke_id_bytes[5U];
    size_t invoke_id_length_bytes = 0U;
    uint32_t value = invoke_id;
    UnitLabMmsBerElement invoke_id_element;

    do {
        invoke_id_bytes[sizeof(invoke_id_bytes) - 1U - invoke_id_length_bytes] = (uint8_t)(value & 0xFFU);
        invoke_id_length_bytes++;
        value >>= 8U;
    } while (value != 0U && invoke_id_length_bytes < sizeof(invoke_id_bytes));

    if (invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes] & 0x80U) {
        if (sizeof(invoke_id_bytes) == invoke_id_length_bytes) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Confirmed response invokeID encoding failed.");
            return 0;
        }
        invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes - 1U] = 0x00U;
        invoke_id_length_bytes++;
    }

    unitlab_mms_ber_element_init(&invoke_id_element);
    invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    invoke_id_element.tag.constructed = 0;
    invoke_id_element.tag.tag_number = 2U;
    invoke_id_element.value_bytes = &invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes];
    invoke_id_element.value_length = invoke_id_length_bytes;
    return unitlab_mms_ber_write(&invoke_id_element, buffer, buffer_length, encoded_length, diagnostic);
}

static const UnitLabIedModelSignal* server_runtime_find_signal_by_object_reference(const UnitLabMmsServerRuntime* server_runtime, const char* object_reference)
{
    if (server_runtime == NULL || server_runtime->model_plan == NULL || object_reference == NULL || object_reference[0] == '\0') {
        return NULL;
    }
    if (server_runtime->model_plan->signals == NULL || server_runtime->model_plan->signal_count == 0U) {
        return NULL;
    }
    for (size_t index = 0U; index < server_runtime->model_plan->signal_count; index++) {
        const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[index];
        if (strcmp(signal->object_reference, object_reference) == 0) {
            return signal;
        }
    }
    return NULL;
}

static int server_runtime_parse_int32_value(const char* source, int32_t* value)
{
    char* end = NULL;
    long parsed = strtol(source, &end, 10);
    if (source == NULL || value == NULL || source == end || end == NULL || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return 0;
    }
    *value = (int32_t)parsed;
    return 1;
}

static int server_runtime_encode_signed_integer(int32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t bytes[5U];
    size_t length = 0U;
    uint32_t raw = (uint32_t)value;
    int negative = value < 0;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    do {
        bytes[sizeof(bytes) - 1U - length] = (uint8_t)(raw & 0xFFU);
        length++;
        raw >>= 8U;
    } while (raw != 0U && raw != UINT32_MAX && length < sizeof(bytes));

    if (negative) {
        while (length < sizeof(bytes) && (bytes[sizeof(bytes) - length] & 0x80U) == 0U) {
            bytes[sizeof(bytes) - length - 1U] = 0xFFU;
            length++;
        }
    } else if (bytes[sizeof(bytes) - length] & 0x80U) {
        if (length == sizeof(bytes)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding is too large.");
            return 0;
        }
        bytes[sizeof(bytes) - length - 1U] = 0x00U;
        length++;
    }

    if (buffer_length < length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Integer encoding buffer is too small.");
        return 0;
    }
    memcpy(buffer, &bytes[sizeof(bytes) - length], length);
    *encoded_length = length;
    return 1;
}

static int server_runtime_encode_mms_data_value(
    const UnitLabIedModelSignal* signal,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement element;
    int32_t integer_value = 0;
    uint8_t integer_bytes[5U];
    size_t integer_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (signal == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Read response value encoding requires a signal and output buffer.");
        return 0;
    }

    unitlab_mms_ber_element_init(&element);
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN: {
            uint8_t boolean_value;

            if (strcmp(signal->initial_value, "true") == 0) {
                boolean_value = 0xFFU;
            } else if (strcmp(signal->initial_value, "false") == 0) {
                boolean_value = 0x00U;
            } else {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Boolean read response value is invalid.");
                return 0;
            }
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 1U;
            element.value_bytes = &boolean_value;
            element.value_length = 1U;
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        }
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            if (!server_runtime_parse_int32_value(signal->initial_value, &integer_value)) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Integer read response value is invalid.");
                return 0;
            }
            if (!server_runtime_encode_signed_integer(integer_value, integer_bytes, sizeof(integer_bytes), &integer_length, diagnostic)) {
                return 0;
            }
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 2U;
            element.value_bytes = &integer_bytes[sizeof(integer_bytes) - integer_length];
            element.value_length = integer_length;
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 26U;
            element.value_bytes = (const uint8_t*)signal->initial_value;
            element.value_length = strlen(signal->initial_value);
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        case UNITLAB_IED_FIXTURE_VALUE_NULL:
            element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            element.tag.constructed = 0;
            element.tag.tag_number = 5U;
            element.value_bytes = NULL;
            element.value_length = 0U;
            return unitlab_mms_ber_write(&element, buffer, buffer_length, encoded_length, diagnostic);
        case UNITLAB_IED_FIXTURE_VALUE_REAL:
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Real read response values are not yet supported by the native wire server.");
            return 0;
        case UNITLAB_IED_FIXTURE_VALUE_UNKNOWN:
        default:
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Read response value kind is unsupported.");
            return 0;
    }
}

static int server_runtime_resolve_read_response_value(UnitLabMmsServerRuntime* server_runtime, const char* object_reference, const UnitLabIedModelSignal** signal_out, UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabIedModelSignal* signal = NULL;

    if (signal_out != NULL) {
        *signal_out = NULL;
    }
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    signal = server_runtime_find_signal_by_object_reference(server_runtime, object_reference);
    if (signal == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Requested object reference is not present in the loaded model plan.");
        return 0;
    }
    if (signal_out != NULL) {
        *signal_out = signal;
    }
    return 1;
}


int unitlab_mms_server_runtime_apply_model_plan(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedModelPlan* plan)
{
    if (server_runtime == NULL) {
        return 0;
    }
    server_runtime->model_plan = plan;
    unitlab_mms_initiate_response_profile_apply_model_plan(&server_runtime->initiate_response_profile, plan);
    server_runtime->read_response_value[0] = '\0';
    server_runtime->read_response_value_length = 0U;
    server_runtime->has_read_response_value = 0;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

static int server_runtime_build_read_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t value_bytes[128U];
    uint8_t access_result_value_bytes[160U];
    uint8_t list_of_access_result_bytes[192U];
    uint8_t read_response_body_bytes[224U];
    uint8_t read_response_sequence_bytes[256U];
    uint8_t invoke_id_element_bytes[16U];
    size_t value_length = 0U;
    size_t access_result_value_length = 0U;
    size_t list_of_access_result_length = 0U;
    size_t read_response_body_length = 0U;
    size_t read_response_sequence_length = 0U;
    size_t invoke_id_length = 0U;
    const UnitLabIedModelSignal* signal = NULL;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Read response service buffer and encoded_length are required.");
        return 0;
    }

    if (!server_runtime_resolve_read_response_value(server_runtime, server_runtime->pending_request.object_reference, &signal, diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_mms_data_value(signal, value_bytes, sizeof(value_bytes), &value_length, diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            value_bytes,
            value_length,
            access_result_value_bytes,
            sizeof(access_result_value_bytes),
            &access_result_value_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            access_result_value_bytes,
            access_result_value_length,
            list_of_access_result_bytes,
            sizeof(list_of_access_result_bytes),
            &list_of_access_result_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            4U,
            list_of_access_result_bytes,
            list_of_access_result_length,
            read_response_body_bytes,
            sizeof(read_response_body_bytes),
            &read_response_body_length,
            diagnostic)) {
        return 0;
    }

    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            invoke_id_element_bytes,
            sizeof(invoke_id_element_bytes),
            &invoke_id_length,
            diagnostic)) {
        return 0;
    }

    if (invoke_id_length + read_response_body_length > sizeof(read_response_sequence_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Read response service buffer is too small.");
        return 0;
    }
    memcpy(read_response_sequence_bytes, invoke_id_element_bytes, invoke_id_length);
    memcpy(read_response_sequence_bytes + invoke_id_length, read_response_body_bytes, read_response_body_length);
    read_response_sequence_length = invoke_id_length + read_response_body_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            read_response_sequence_bytes,
            read_response_sequence_length,
            buffer,
            buffer_length,
            encoded_length,
            diagnostic)) {
        return 0;
    }
    return 1;
}

static int server_runtime_decode_transport_to_wire_pdu(
    const uint8_t* buffer,
    size_t buffer_length,
    size_t* consumed_length,
    UnitLabMmsPdu* wire_pdu,
    UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsTransportFrame transport_frame;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    const uint8_t* presentation_bytes = NULL;
    size_t presentation_length = 0U;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t pdu_consumed_length = 0U;

    if (!unitlab_mms_transport_frame_decode(&transport_frame, buffer, buffer_length, &transport_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (transport_frame.cotp.user_data_length == 0U || transport_frame.cotp.user_data == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Transport frame is missing session bytes.");
        return 0;
    }
    unitlab_mms_session_spdu_init(&session_spdu);
    if (!unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (session_consumed_length != transport_frame.cotp.user_data_length) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing session bytes.");
        return 0;
    }
    if (session_spdu.raw_parameter_length == 0U || session_spdu.raw_parameter_bytes == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Session SPDU is missing presentation bytes.");
        return 0;
    }
    presentation_bytes = session_spdu.raw_parameter_bytes;
    presentation_length = session_spdu.raw_parameter_length;
    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    if (unitlab_mms_presentation_decode(&presentation_apdu, presentation_bytes, presentation_length, &presentation_consumed_length, &operation_result->diagnostic)) {
        if (presentation_consumed_length != presentation_length) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing presentation bytes.");
            return 0;
        }
        if (presentation_apdu.payload_length == 0U || presentation_apdu.payload_bytes == NULL) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Presentation User-data is missing MMS bytes.");
            return 0;
        }
        unitlab_mms_pdu_init(wire_pdu);
        if (unitlab_mms_pdu_decode(wire_pdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &pdu_consumed_length, &operation_result->diagnostic)) {
            if (pdu_consumed_length != presentation_apdu.payload_length) {
                operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing MMS bytes.");
                return 0;
            }
            *consumed_length = transport_consumed_length;
            return 1;
        }

        {
            UnitLabMmsAcseApdu acse_apdu;
            size_t acse_consumed_length = 0U;

            unitlab_mms_acse_apdu_init(&acse_apdu);
            if (!unitlab_mms_acse_decode(&acse_apdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &acse_consumed_length, &operation_result->diagnostic)) {
                return 0;
            }
            if (acse_consumed_length != presentation_apdu.payload_length) {
                operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing ACSE bytes.");
                return 0;
            }
            if (acse_apdu.kind != UNITLAB_MMS_ACSE_APDU_AARQ) {
                operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
                snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association request bytes must carry an ACSE AARQ or MMS initiate request.");
                return 0;
            }
            unitlab_mms_pdu_init(wire_pdu);
            wire_pdu->kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
            *consumed_length = transport_consumed_length;
            return 1;
        }
    }
    unitlab_mms_pdu_init(wire_pdu);
    if (unitlab_mms_pdu_decode(wire_pdu, presentation_bytes, presentation_length, &pdu_consumed_length, &operation_result->diagnostic)) {
        if (pdu_consumed_length != presentation_length) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing MMS bytes.");
            return 0;
        }
        *consumed_length = transport_consumed_length;
        return 1;
    }

    {
        UnitLabMmsAcseApdu acse_apdu;
        size_t acse_consumed_length = 0U;

        unitlab_mms_acse_apdu_init(&acse_apdu);
        if (!unitlab_mms_acse_decode(&acse_apdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &acse_consumed_length, &operation_result->diagnostic)) {
            return 0;
        }
        if (acse_consumed_length != presentation_apdu.payload_length) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing ACSE bytes.");
            return 0;
        }
        if (acse_apdu.kind != UNITLAB_MMS_ACSE_APDU_AARQ) {
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association request bytes must carry an ACSE AARQ or MMS initiate request.");
            return 0;
        }
        unitlab_mms_pdu_init(wire_pdu);
        wire_pdu->kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
        *consumed_length = transport_consumed_length;
        return 1;
    }
}

void unitlab_mms_server_runtime_init(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    memset(&server_runtime->config, 0, sizeof(server_runtime->config));
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_IDLE;
    server_runtime->model_plan = NULL;
    unitlab_mms_session_init(&server_runtime->session);
    unitlab_mms_pending_request_init(&server_runtime->pending_request);
    unitlab_mms_initiate_response_profile_init(&server_runtime->initiate_response_profile);
    server_runtime->read_response_value[0] = '\0';
    server_runtime->read_response_value_length = 0U;
    server_runtime->has_read_response_value = 0;
    unitlab_iec61850_report_control_init(&server_runtime->report_control);
    unitlab_mms_transport_exchange_init(&server_runtime->transport);
    unitlab_mms_operation_result_init(&server_runtime->last_result);
    unitlab_mms_runtime_snapshot_init(&server_runtime->snapshot);
    unitlab_mms_runtime_snapshot_capture(
        &server_runtime->snapshot,
        &server_runtime->session,
        &server_runtime->report_control,
        &server_runtime->transport,
        &server_runtime->last_result);
}

int unitlab_mms_server_runtime_prepare(UnitLabMmsServerRuntime* server_runtime, const UnitLabIedServerConfig* config, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_IDLE && server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_STOPPED) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be idle or stopped before prepare.");
        return 0;
    }
    if (!server_runtime_validate_config(config, diagnostic)) {
        server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_FAILED;
        return 0;
    }
    server_runtime->config = *config;
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_PREPARED;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_start(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_PREPARED) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be prepared before start.");
        return 0;
    }
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_RUNNING;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_stop(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (server_runtime->state == UNITLAB_MMS_SERVER_RUNTIME_IDLE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime cannot stop before prepare.");
        return 0;
    }
    if (server_runtime->state == UNITLAB_MMS_SERVER_RUNTIME_STOPPED) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_PREPARED && server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime cannot stop from the current state.");
        return 0;
    }
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_STOPPED;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

static int server_runtime_prepare_confirmed_response_pdu(
    const UnitLabMmsServerRuntime* server_runtime,
    const uint8_t* service_bytes,
    size_t service_length,
    UnitLabMmsPdu* response_pdu,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL || response_pdu == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime and response PDU are required.");
        return 0;
    }
    if (server_runtime->pending_request.state != UNITLAB_MMS_PENDING_REQUEST_ACTIVE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Pending request must be active before building a response.");
        return 0;
    }
    if (server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_READ && server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_WRITE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Only first-slice READ/WRITE responses are supported.");
        return 0;
    }
    unitlab_mms_pdu_init(response_pdu);
    response_pdu->kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    response_pdu->has_invoke_id = 1;
    response_pdu->invoke_id = server_runtime->pending_request.invoke_id;
    response_pdu->has_service = 1;
    response_pdu->service_kind = server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_READ ? UNITLAB_MMS_SERVICE_READ : UNITLAB_MMS_SERVICE_WRITE;
    response_pdu->pdu_bytes = service_bytes;
    response_pdu->pdu_length = service_length;
    return 1;
}

static int server_runtime_require_running(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be running for report-control operations.");
        return 0;
    }
    return 1;
}

int unitlab_mms_server_runtime_reserve_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_reserve(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_enable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_enable(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_request_general_interrogation(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_request_gi(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_disable_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_disable(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_release_report_control(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required.");
        return 0;
    }
    if (!server_runtime_require_running(server_runtime, diagnostic)) {
        return 0;
    }
    if (!unitlab_iec61850_report_control_release(&server_runtime->report_control, diagnostic)) {
        return 0;
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_build_confirmed_response_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* service_bytes, size_t service_length, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsPdu response_pdu;
    uint8_t synthesized_service_bytes[64U];
    size_t synthesized_service_length = 0U;
    size_t response_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime, buffer, and encoded_length are required.");
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BAD_STATE, "Server runtime must be running before building response bytes.");
        return 0;
    }
    if (service_length == 0U) {
        if (server_runtime->pending_request.kind == UNITLAB_MMS_REQUEST_READ) {
            if (!server_runtime_build_read_response_service(
                    server_runtime,
                    server_runtime->pending_request.invoke_id,
                    synthesized_service_bytes,
                    sizeof(synthesized_service_bytes),
                    &synthesized_service_length,
                    diagnostic)) {
                return 0;
            }
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        } else {
            uint8_t invoke_id_bytes[5U];
            size_t invoke_id_length_bytes = 0U;
            uint32_t value = server_runtime->pending_request.invoke_id;
            UnitLabMmsBerElement invoke_id_element;
            UnitLabMmsBerElement service_element;
            size_t invoke_id_length = 0U;
            size_t service_encoded_length = 0U;

            do {
                invoke_id_bytes[sizeof(invoke_id_bytes) - 1U - invoke_id_length_bytes] = (uint8_t)(value & 0xFFU);
                invoke_id_length_bytes++;
                value >>= 8U;
            } while (value != 0U && invoke_id_length_bytes < sizeof(invoke_id_bytes));
            if (invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes] & 0x80U) {
                if (sizeof(invoke_id_bytes) == invoke_id_length_bytes) {
                    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Confirmed response invokeID encoding failed.");
                    return 0;
                }
                invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes - 1U] = 0x00U;
                invoke_id_length_bytes++;
            }
            unitlab_mms_ber_element_init(&invoke_id_element);
            invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
            invoke_id_element.tag.constructed = 0;
            invoke_id_element.tag.tag_number = 2U;
            invoke_id_element.value_bytes = &invoke_id_bytes[sizeof(invoke_id_bytes) - invoke_id_length_bytes];
            invoke_id_element.value_length = invoke_id_length_bytes;
            if (!unitlab_mms_ber_write(&invoke_id_element, synthesized_service_bytes, sizeof(synthesized_service_bytes), &invoke_id_length, diagnostic)) {
                return 0;
            }
            unitlab_mms_ber_element_init(&service_element);
            service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
            service_element.tag.constructed = 0;
            service_element.tag.tag_number = 5U;
            service_element.value_bytes = NULL;
            service_element.value_length = 0U;
            if (!unitlab_mms_ber_write(&service_element, &synthesized_service_bytes[invoke_id_length], sizeof(synthesized_service_bytes) - invoke_id_length, &service_encoded_length, diagnostic)) {
                return 0;
            }
            synthesized_service_length = invoke_id_length + service_encoded_length;
            service_bytes = synthesized_service_bytes;
            service_length = synthesized_service_length;
        }
    }
    if (!server_runtime_prepare_confirmed_response_pdu(server_runtime, service_bytes, service_length, &response_pdu, diagnostic)) {
        return 0;
    }

    if (!unitlab_mms_build_confirmed_response_frame(&response_pdu, server_runtime->wire_scratch, sizeof(server_runtime->wire_scratch), buffer, buffer_length, &response_length, diagnostic)) {
        return 0;
    }
    if (!unitlab_mms_transport_exchange_bind_response(&server_runtime->transport, buffer, buffer_length, diagnostic)) {
        return 0;
    }
    if (!unitlab_mms_transport_exchange_set_response_length(&server_runtime->transport, response_length, diagnostic)) {
        return 0;
    }
    *encoded_length = response_length;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    unitlab_mms_operation_result_from_trace(
        &server_runtime->last_result,
        1,
        diagnostic,
        &server_runtime->transport.event_log,
        &server_runtime->transport.last_event);
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

int unitlab_mms_server_runtime_apply_incoming_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsPdu wire_pdu;
    size_t transport_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || consumed_length == NULL || operation_result == NULL) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime, buffer, consumed length, and operation result are required.");
            server_runtime_fail_and_capture(server_runtime, operation_result);
        }
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        unitlab_mms_operation_result_init(operation_result);
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying incoming bytes.");
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }

    if (server_runtime->session.state != UNITLAB_MMS_SESSION_DISCONNECTED) {
        unitlab_mms_session_init(&server_runtime->session);
        unitlab_mms_pending_request_init(&server_runtime->pending_request);
        unitlab_mms_transport_exchange_init(&server_runtime->transport);
    }

    unitlab_mms_operation_result_init(operation_result);
    unitlab_mms_pdu_init(&wire_pdu);
    if (!server_runtime_decode_transport_to_wire_pdu(buffer, buffer_length, &transport_consumed_length, &wire_pdu, operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_transport_exchange_bind_request(
            &server_runtime->transport,
            buffer,
            transport_consumed_length,
            wire_pdu.has_invoke_id ? wire_pdu.invoke_id : 0U,
            &operation_result->diagnostic)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            &wire_pdu,
            operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    *consumed_length = transport_consumed_length;
    return 1;
}

int unitlab_mms_server_runtime_apply_association_request_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsPdu wire_pdu;
    size_t transport_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || consumed_length == NULL || operation_result == NULL) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime, buffer, consumed length, and operation result are required.");
            server_runtime_fail_and_capture(server_runtime, operation_result);
        }
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        unitlab_mms_operation_result_init(operation_result);
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying association request bytes.");
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }

    if (server_runtime->session.state != UNITLAB_MMS_SESSION_DISCONNECTED) {
        unitlab_mms_session_init(&server_runtime->session);
        unitlab_mms_pending_request_init(&server_runtime->pending_request);
        unitlab_mms_transport_exchange_init(&server_runtime->transport);
    }

    unitlab_mms_operation_result_init(operation_result);
    unitlab_mms_pdu_init(&wire_pdu);
    if (!server_runtime_decode_transport_to_wire_pdu(buffer, buffer_length, &transport_consumed_length, &wire_pdu, operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (wire_pdu.kind != UNITLAB_MMS_PDU_INITIATE_REQUEST) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association request bytes must carry an MMS initiate request.");
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_transport_exchange_bind_request(
            &server_runtime->transport,
            buffer,
            transport_consumed_length,
            0U,
            &operation_result->diagnostic)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            &wire_pdu,
            operation_result)) {
        server_runtime_fail_and_capture(server_runtime, operation_result);
        return 0;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    *consumed_length = transport_consumed_length;
    return 1;
}

int unitlab_mms_server_runtime_apply_association_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    return unitlab_mms_server_runtime_apply_incoming_bytes(server_runtime, buffer, buffer_length, consumed_length, operation_result);
}

int unitlab_mms_server_runtime_apply_wire_pdu(UnitLabMmsServerRuntime* server_runtime, const UnitLabMmsPdu* wire_pdu, UnitLabMmsOperationResult* operation_result)
{
    if (server_runtime == NULL) {
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying wire PDUs.");
        }
        return 0;
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            wire_pdu,
            operation_result)) {
        if (operation_result != NULL) {
            server_runtime->last_result = *operation_result;
        }
        unitlab_mms_server_runtime_capture_snapshot(server_runtime);
        return 0;
    }
    if (operation_result != NULL) {
        server_runtime->last_result = *operation_result;
    }
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    return 1;
}

void unitlab_mms_server_runtime_capture_snapshot(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    unitlab_mms_runtime_snapshot_capture(
        &server_runtime->snapshot,
        &server_runtime->session,
        &server_runtime->report_control,
        &server_runtime->transport,
        &server_runtime->last_result);
}
