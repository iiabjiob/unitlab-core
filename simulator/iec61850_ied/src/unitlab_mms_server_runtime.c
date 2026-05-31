#include "unitlab_mms_server_runtime.h"

#include <string.h>
#include <stdio.h>

#include "wire/presentation/unitlab_mms_presentation.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
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

int unitlab_mms_server_runtime_apply_association_bytes(UnitLabMmsServerRuntime* server_runtime, const uint8_t* buffer, size_t buffer_length, size_t* consumed_length, UnitLabMmsOperationResult* operation_result)
{
    UnitLabMmsTransportFrame transport_frame;
    UnitLabMmsSessionSpdu session_spdu;
    UnitLabMmsPresentationApdu presentation_apdu;
    UnitLabMmsPdu wire_pdu;
    size_t transport_consumed_length = 0U;
    size_t session_consumed_length = 0U;
    size_t presentation_consumed_length = 0U;
    size_t pdu_consumed_length = 0U;

    if (consumed_length != NULL) {
        *consumed_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || consumed_length == NULL || operation_result == NULL) {
        if (operation_result != NULL) {
            unitlab_mms_operation_result_init(operation_result);
            operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime, buffer, consumed length, and operation result are required.");
        }
        return 0;
    }
    if (server_runtime->state != UNITLAB_MMS_SERVER_RUNTIME_RUNNING) {
        unitlab_mms_operation_result_init(operation_result);
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_BAD_STATE;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Server runtime must be running before applying association bytes.");
        return 0;
    }

    unitlab_mms_transport_frame_init(&transport_frame);
    unitlab_mms_session_spdu_init(&session_spdu);
    unitlab_mms_presentation_apdu_init(&presentation_apdu);
    unitlab_mms_pdu_init(&wire_pdu);
    unitlab_mms_operation_result_init(operation_result);

    if (!unitlab_mms_transport_frame_decode(&transport_frame, buffer, buffer_length, &transport_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (transport_frame.cotp.user_data_length == 0U || transport_frame.cotp.user_data == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Transport frame is missing session bytes.");
        return 0;
    }
    if (!unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (session_consumed_length != transport_frame.cotp.user_data_length) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association bytes contain trailing session bytes.");
        return 0;
    }
    if (session_spdu.raw_parameter_length == 0U || session_spdu.raw_parameter_bytes == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Session SPDU is missing presentation bytes.");
        return 0;
    }
    if (!unitlab_mms_presentation_decode(&presentation_apdu, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &presentation_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (presentation_consumed_length != session_spdu.raw_parameter_length) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association bytes contain trailing presentation bytes.");
        return 0;
    }
    if (presentation_apdu.payload_length == 0U || presentation_apdu.payload_bytes == NULL) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Presentation User-data is missing MMS bytes.");
        return 0;
    }
    if (!unitlab_mms_pdu_decode(&wire_pdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &pdu_consumed_length, &operation_result->diagnostic)) {
        return 0;
    }
    if (pdu_consumed_length != presentation_apdu.payload_length) {
        operation_result->diagnostic.code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
        snprintf(operation_result->diagnostic.message, sizeof(operation_result->diagnostic.message), "%s", "Association bytes contain trailing MMS bytes.");
        return 0;
    }
    if (!unitlab_mms_runtime_apply_wire_pdu_with_report_control(
            &server_runtime->session,
            &server_runtime->pending_request,
            &server_runtime->report_control,
            &wire_pdu,
            operation_result)) {
        server_runtime->last_result = *operation_result;
        unitlab_mms_server_runtime_capture_snapshot(server_runtime);
        return 0;
    }
    server_runtime->last_result = *operation_result;
    unitlab_mms_server_runtime_capture_snapshot(server_runtime);
    *consumed_length = transport_consumed_length;
    return 1;
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
