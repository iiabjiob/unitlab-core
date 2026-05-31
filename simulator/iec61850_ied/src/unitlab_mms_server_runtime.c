#include "unitlab_mms_server_runtime.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"
#include "unitlab_mms_runtime_bridge.h"

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
    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    if (!unitlab_mms_presentation_decode(&presentation_apdu, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &presentation_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (presentation_consumed_length != session_spdu.raw_parameter_length) {
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
    if (!unitlab_mms_pdu_decode(wire_pdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &pdu_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (pdu_consumed_length != presentation_apdu.payload_length) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Incoming bytes contain trailing MMS bytes.");
        return 0;
    }
    *consumed_length = transport_consumed_length;
    return 1;
}

void unitlab_mms_server_runtime_init(UnitLabMmsServerRuntime* server_runtime)
{
    if (server_runtime == NULL) {
        return;
    }
    memset(&server_runtime->config, 0, sizeof(server_runtime->config));
    server_runtime->state = UNITLAB_MMS_SERVER_RUNTIME_IDLE;
    unitlab_mms_session_init(&server_runtime->session);
    unitlab_mms_pending_request_init(&server_runtime->pending_request);
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
    UnitLabMmsWireAssociationFixture fixture;
    uint8_t* response_payload = NULL;
    size_t response_payload_length = 0U;
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
    if (!server_runtime_prepare_confirmed_response_pdu(server_runtime, service_bytes, service_length, &response_pdu, diagnostic)) {
        return 0;
    }
    response_payload = (uint8_t*)malloc(buffer_length);
    if (response_payload == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Response scratch allocation failed.");
        return 0;
    }

    if (!unitlab_mms_pdu_encode(&response_pdu, response_payload, buffer_length, &response_payload_length, diagnostic)) {
        free(response_payload);
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
    fixture.presentation.payload_bytes = response_payload;
    fixture.presentation.payload_length = response_payload_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.user_data = response_payload;
    fixture.transport.cotp.user_data_length = response_payload_length;

    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, &response_length, diagnostic)) {
        free(response_payload);
        return 0;
    }
    free(response_payload);
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
    server_runtime_set_diagnostic(&operation_result->diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    operation_result->ok = 1;
    unitlab_mms_operation_result_from_trace(
        operation_result,
        1,
        &operation_result->diagnostic,
        &server_runtime->transport.event_log,
        &server_runtime->transport.last_event);
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
