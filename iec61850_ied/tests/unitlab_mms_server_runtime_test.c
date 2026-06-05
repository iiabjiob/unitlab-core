#include <assert.h>

#include "server/unitlab_mms_server_runtime.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

#include <string.h>
#include <stdio.h>

static UnitLabMmsPdu make_information_report_pdu(void)
{
    UnitLabMmsPdu pdu;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    pdu.has_service = 1;
    pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    return pdu;
}

static int build_information_report_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t scratch[256U];
    return unitlab_mms_build_information_report_frame("RPT", 0U, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, diagnostic);
}

static int contains_bytes(const uint8_t* haystack, size_t haystack_length, const uint8_t* needle, size_t needle_length)
{
    if (haystack == NULL || needle == NULL || needle_length == 0U || haystack_length < needle_length) {
        return 0;
    }
    for (size_t index = 0U; index + needle_length <= haystack_length; index++) {
        if (memcmp(&haystack[index], needle, needle_length) == 0) {
            return 1;
        }
    }
    return 0;
}

static int build_get_name_list_request_association_bytes(const uint8_t* request_body_bytes, size_t request_body_length, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic);


static int build_initiate_request_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsAcseApdu acse_apdu;
    uint8_t acse_payload[3U] = { 0x80U, 0x01U, 0x01U };
    uint8_t acse_encoded[16U];
    size_t acse_length = 0U;
    size_t frame_length = 0U;

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_payload;
    acse_apdu.apdu_length = sizeof(acse_payload);

    if (!unitlab_mms_acse_encode(&acse_apdu, acse_encoded, sizeof(acse_encoded), &acse_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_association_frame_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = acse_encoded;
    fixture.presentation.payload_length = acse_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;

    if (!unitlab_mms_association_frame_encode(&fixture, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }
    *encoded_length = frame_length;
    return 1;
}

static int build_model_read_request_association_bytes(const char* raw_object_reference, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    char domain_id[128U];
    char item_id[128U];
    const char* separator = NULL;
    size_t domain_length = 0U;
    uint8_t scratch[256U];

    if (raw_object_reference == NULL || buffer == NULL || encoded_length == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return 0;
    }

    separator = strchr(raw_object_reference, '$');
    if (separator == NULL || separator == raw_object_reference) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
        }
        return 0;
    }

    domain_length = (size_t)(separator - raw_object_reference);
    if (domain_length >= sizeof(domain_id)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
        }
        return 0;
    }
    memcpy(domain_id, raw_object_reference, domain_length);
    domain_id[domain_length] = '\0';

    if (strlen(separator + 1U) >= sizeof(item_id)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
        }
        return 0;
    }
    snprintf(item_id, sizeof(item_id), "%s", separator + 1U);

    return unitlab_mms_build_read_request_frame(domain_id, item_id, invoke_id, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, diagnostic);
}

static void test_server_runtime_apply_association_request_bytes_accepts_acse_aarq(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_initiate_request_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == wire_length);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(server_runtime.session.active_invoke_id == 1U);
    assert(server_runtime.last_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);
}

static void test_server_runtime_apply_association_request_bytes_accepts_captured_iedscout_aarq(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    static const uint8_t wire_bytes[] = { 0x03U, 0x00U, 0x00U, 0xD3U, 0x02U, 0xF0U, 0x80U, 0x0DU, 0xCAU, 0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x33U, 0x02U, 0x00U, 0x01U, 0x34U, 0x02U, 0x00U, 0x01U, 0xC1U, 0xB4U, 0x31U, 0x81U, 0xB1U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x01U, 0xA2U, 0x81U, 0xA9U, 0x81U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0x82U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0xA4U, 0x23U, 0x30U, 0x0FU, 0x02U, 0x01U, 0x01U, 0x06U, 0x04U, 0x52U, 0x01U, 0x00U, 0x01U, 0x30U, 0x04U, 0x06U, 0x02U, 0x51U, 0x01U, 0x30U, 0x10U, 0x02U, 0x01U, 0x03U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x01U, 0x30U, 0x04U, 0x06U, 0x02U, 0x51U, 0x01U, 0x61U, 0x76U, 0x30U, 0x74U, 0x02U, 0x01U, 0x01U, 0xA0U, 0x6FU, 0x60U, 0x6DU, 0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U, 0xA2U, 0x07U, 0x06U, 0x05U, 0x29U, 0x01U, 0x87U, 0x67U, 0x01U, 0xA3U, 0x03U, 0x02U, 0x01U, 0x0CU, 0xA4U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA5U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA6U, 0x06U, 0x06U, 0x04U, 0x29U, 0x01U, 0x87U, 0x67U, 0xA7U, 0x03U, 0x02U, 0x01U, 0x0CU, 0xA8U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA9U, 0x03U, 0x02U, 0x01U, 0x00U, 0xBEU, 0x33U, 0x28U, 0x31U, 0x06U, 0x02U, 0x51U, 0x01U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x28U, 0xA8U, 0x26U, 0x80U, 0x03U, 0x00U, 0xFDU, 0xE8U, 0x81U, 0x01U, 0x0AU, 0x82U, 0x01U, 0x0AU, 0x83U, 0x01U, 0x05U, 0xA4U, 0x16U, 0x80U, 0x01U, 0x01U, 0x81U, 0x03U, 0x05U, 0xF1U, 0x00U, 0x82U, 0x0CU, 0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xEDU, 0x18U };
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, wire_bytes, sizeof(wire_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(wire_bytes));
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == sizeof(wire_bytes));
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(server_runtime.session.active_invoke_id == 1U);
    assert(server_runtime.last_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_SESSION_BEGIN_ASSOCIATION);
}

static void test_server_runtime_build_association_response_matches_reference_capture(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t request_bytes[256];
    uint8_t response_bytes[256];
    size_t request_length = 0U;
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    static const uint8_t expected_response[] = { 0x03U, 0x00U, 0x00U, 0x8FU, 0x02U, 0xF0U, 0x80U, 0x0EU, 0x86U, 0x05U, 0x06U, 0x13U, 0x01U, 0x00U, 0x16U, 0x01U, 0x02U, 0x14U, 0x02U, 0x00U, 0x02U, 0x34U, 0x02U, 0x00U, 0x01U, 0xC1U, 0x74U, 0x31U, 0x72U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x01U, 0xA2U, 0x6BU, 0x83U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0xA5U, 0x12U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x30U, 0x07U, 0x80U, 0x01U, 0x00U, 0x81U, 0x02U, 0x51U, 0x01U, 0x61U, 0x4FU, 0x30U, 0x4DU, 0x02U, 0x01U, 0x01U, 0xA0U, 0x48U, 0x61U, 0x46U, 0xA1U, 0x07U, 0x06U, 0x05U, 0x28U, 0xCAU, 0x22U, 0x02U, 0x03U, 0xA2U, 0x03U, 0x02U, 0x01U, 0x00U, 0xA3U, 0x05U, 0xA1U, 0x03U, 0x02U, 0x01U, 0x00U, 0xBEU, 0x2FU, 0x28U, 0x2DU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x28U, 0xA9U, 0x26U, 0x80U, 0x03U, 0x00U, 0xFDU, 0xE8U, 0x81U, 0x01U, 0x05U, 0x82U, 0x01U, 0x05U, 0x83U, 0x01U, 0x05U, 0xA4U, 0x16U, 0x80U, 0x01U, 0x01U, 0x81U, 0x03U, 0x05U, 0xF1U, 0x00U, 0x82U, 0x0CU, 0x03U, 0xEEU, 0x1CU, 0x00U, 0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x40U, 0xEDU, 0x18U };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_initiate_request_association_bytes(request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(consumed_length == request_length);

    assert(unitlab_mms_build_association_response_frame_with_profile(&server_runtime.initiate_response_profile, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length == sizeof(expected_response));
    assert(memcmp(response_bytes, expected_response, sizeof(expected_response)) == 0);
}

static void test_server_runtime_apply_association_then_confirmed_request_keeps_session_associated(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t association_bytes[256U];
    uint8_t request_bytes[256U];
    size_t association_length = 0U;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    const uint8_t request_payload[] = {
        0x30U, 0x0CU,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x02U,
        0xA1U, 0x05U, 0x81U, 0x03U, 'L', 'D', '0'
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_initiate_request_association_bytes(association_bytes, sizeof(association_bytes), &association_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, association_bytes, association_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == association_length);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATING);
    assert(unitlab_mms_session_complete_association(&server_runtime.session, server_runtime.session.active_invoke_id, &diagnostic));
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATED);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 61U, request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == request_length);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_ASSOCIATED);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.invoke_id == 61U);
}

static void test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    assert(build_information_report_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_association_request_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result) == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED);
}


static void test_server_runtime_apply_model_plan_sets_active_model(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabIedModelPlan plan;

    memset(&server_runtime, 0, sizeof(server_runtime));
    memset(&plan, 0, sizeof(plan));

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(server_runtime.model_plan == &plan);
    assert(server_runtime.has_read_response_value == 0);
    assert(server_runtime.read_response_value_length == 0U);
}

static void test_server_runtime_init_captures_default_snapshot(void)
{
    UnitLabMmsServerRuntime server_runtime;

    unitlab_mms_server_runtime_init(&server_runtime);

    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_IDLE);
    assert(server_runtime.session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
    assert(server_runtime.snapshot.session.state == UNITLAB_MMS_SESSION_DISCONNECTED);
    assert(server_runtime.snapshot.report_control_state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED);
}

static void test_server_runtime_prepare_start_stop(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_PREPARED);
    assert(server_runtime.config.port == 102);

    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);

    assert(unitlab_mms_server_runtime_stop(&server_runtime, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_STOPPED);
}

static void test_server_runtime_apply_wire_pdu_requires_running_state(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsPdu wire_pdu = make_information_report_pdu();
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_operation_result_init(&operation_result);

    assert(!unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result));
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BAD_STATE);

    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED);
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED);
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result));
    assert(operation_result.ok == 1);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);
}

static void test_wire_builder_builds_confirmed_response_frame_roundtrips(void)
{
    UnitLabMmsPdu response_pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    uint8_t response_bytes[256];
    uint8_t response_payload[6] = { 0x02U, 0x01U, 0x29U, 0xA4U, 0x01U, 0xAAU };
    size_t encoded_length = 0U;

    unitlab_mms_pdu_init(&response_pdu);
    response_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    response_pdu.has_invoke_id = 1;
    response_pdu.invoke_id = 41U;
    response_pdu.has_service = 1;
    response_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;
    response_pdu.pdu_bytes = response_payload;
    response_pdu.pdu_length = sizeof(response_payload);

    uint8_t scratch[256];
    assert(unitlab_mms_build_confirmed_response_frame(&response_pdu, scratch, sizeof(scratch), response_bytes, sizeof(response_bytes), &encoded_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);

    {
        UnitLabMmsTransportFrame transport_frame;
        UnitLabMmsSessionSpdu session_spdu;
        UnitLabMmsPresentationApdu presentation_apdu;
        size_t transport_consumed_length = 0U;
        size_t session_consumed_length = 0U;
        size_t presentation_consumed_length = 0U;

        unitlab_mms_transport_frame_init(&transport_frame);
        assert(unitlab_mms_transport_frame_decode(&transport_frame, response_bytes, encoded_length, &transport_consumed_length, &diagnostic));
        assert(transport_consumed_length == encoded_length);
        unitlab_mms_session_spdu_init(&session_spdu);
        assert(unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic));
        assert(session_consumed_length == transport_frame.cotp.user_data_length);
        unitlab_mms_presentation_apdu_init(&presentation_apdu);
        assert(unitlab_mms_presentation_decode(&presentation_apdu, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &presentation_consumed_length, &diagnostic));
        assert(presentation_consumed_length == session_spdu.raw_parameter_length);
        assert(presentation_apdu.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        {
            UnitLabMmsPdu decoded_response_pdu;
            size_t response_pdu_consumed_length = 0U;

            unitlab_mms_pdu_init(&decoded_response_pdu);
            assert(unitlab_mms_pdu_decode(&decoded_response_pdu, presentation_apdu.payload_bytes, presentation_apdu.payload_length, &response_pdu_consumed_length, &diagnostic));
            assert(response_pdu_consumed_length == presentation_apdu.payload_length);
            assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
            assert(decoded_response_pdu.has_invoke_id == 1);
            assert(decoded_response_pdu.invoke_id == 41U);
            assert(decoded_response_pdu.has_service == 1);
            assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
            assert(decoded_response_pdu.pdu_length == sizeof(response_payload));
            assert(memcmp(decoded_response_pdu.pdu_bytes, response_payload, sizeof(response_payload)) == 0);
        }
    }
    (void)response_pdu;
    (void)encoded_length;
}

static void test_server_runtime_build_confirmed_response_bytes_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[1U];
    uint8_t response_bytes[256];
    uint8_t response_payload[6] = { 0x02U, 0x01U, 0x29U, 0xA4U, 0x01U, 0xAAU };
    UnitLabMmsAssociationFrame fixture;
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    strcpy(signals[0].object_reference, "XCBR1.ST.Pos.stVal");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
    strcpy(signals[0].initial_value, "model-read");
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 41U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "XCBR1.ST.Pos.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &encoded_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);
    assert(server_runtime.transport.response_bytes == response_bytes);
    assert(server_runtime.transport.response_length == encoded_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH);

    unitlab_mms_association_frame_init(&fixture);
    assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, encoded_length, &consumed_length, &diagnostic));
    assert(consumed_length == encoded_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(fixture.presentation.payload_length > 0U);
    assert(fixture.presentation.payload_length > sizeof(response_payload));
    assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"model-read", strlen("model-read")) == 1);

    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1234U, &diagnostic));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);

    (void)fixture;
    (void)consumed_length;
}


static void test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_exact_frame(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);

    assert(build_information_report_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == consumed_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);
}

static void test_server_runtime_apply_reference_confirmed_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[1U];
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    strcpy(signals[0].object_reference, "XCBR1.ST.Pos.stVal");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_STRING;
    strcpy(signals[0].initial_value, "model-read");
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    assert(build_model_read_request_association_bytes("XCBR1$ST$Pos$stVal", 3U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    {
        UnitLabMmsAssociationFrame request_fixture;
        UnitLabMmsPdu decoded_request_pdu;
        size_t request_consumed_length = 0U;

        unitlab_mms_association_frame_init(&request_fixture);
        assert(unitlab_mms_association_frame_decode(&request_fixture, wire_bytes, wire_length, &request_consumed_length, &diagnostic));
        assert(request_consumed_length == wire_length);
        assert(request_fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        unitlab_mms_pdu_init(&decoded_request_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_request_pdu, request_fixture.presentation.payload_bytes, request_fixture.presentation.payload_length, &request_consumed_length, &diagnostic));
        assert(decoded_request_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_REQUEST);
        assert(decoded_request_pdu.has_service == 1);
        assert(decoded_request_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(decoded_request_pdu.has_invoke_id == 1);
        assert(decoded_request_pdu.invoke_id == 3U);
    }
    unitlab_mms_operation_result_init(&operation_result);
    {
        int incoming_ok = unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result);
        if (!incoming_ok) {
            fprintf(stderr, "runtime diag: %d %s\n", operation_result.diagnostic.code, operation_result.diagnostic.message);
            }
        assert(incoming_ok);
    }
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.transport.invoke_id == 3U);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 3U);
    assert(strcmp(server_runtime.pending_request.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "stVal") == 0);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);

    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic)) {
        assert(0);
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        assert(fixture.presentation.payload_length > 0U);

        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"model-read", strlen("model-read")) == 1);
    }
}

static void test_server_runtime_build_get_name_list_response_handles_large_directory(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[24U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[4096U];
    const uint8_t request_payload[] = {
        0x30U, 0x09U,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x09U,
        0xA1U, 0x02U, 0x80U, 0x00U
    };
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    for (size_t index = 0U; index < sizeof(logical_devices) / sizeof(logical_devices[0]); index++) {
        snprintf(logical_devices[index].inst, sizeof(logical_devices[index].inst), "LD%02u", (unsigned)index);
    }
    plan.logical_device_count = sizeof(logical_devices) / sizeof(logical_devices[0]);
    plan.logical_devices = logical_devices;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 61U, request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == request_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 9U);
    assert(server_runtime.pending_request.browse_object_scope == 0U);

    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic)) {
        assert(0);
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 64U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LD00", strlen("LD00")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LD23", strlen("LD23")) == 1);
    }
}

static void test_server_runtime_build_confirmed_response_bytes_matches_fixture_style_object_reference(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelSignal signals[1U];
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(signals, 0, sizeof(signals));
    strcpy(signals[0].object_reference, "Pos.stVal");
    signals[0].initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER;
    strcpy(signals[0].initial_value, "0");
    plan.signal_count = 1U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_model_read_request_association_bytes("XCBR1$ST$Pos$stVal", 3U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(strcmp(server_runtime.pending_request.object_reference, "XCBR1.ST.Pos.stVal") == 0);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
}

static void test_server_runtime_apply_confirmed_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    uint8_t response_payload[5U] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x00U };
    UnitLabMmsAssociationFrame fixture;
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 5U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "XCBR1.ST.Pos.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&fixture);
    assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    {
        UnitLabMmsPdu decoded_response_pdu;
        size_t response_pdu_consumed_length = 0U;

        unitlab_mms_pdu_init(&decoded_response_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.has_invoke_id == 1);
        assert(decoded_response_pdu.invoke_id == 5U);
        assert(decoded_response_pdu.has_service == 1);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(decoded_response_pdu.pdu_length == sizeof(response_payload));
        assert(memcmp(decoded_response_pdu.pdu_bytes, response_payload, sizeof(response_payload)) == 0);
    }

    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1234U, &diagnostic));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);

    (void)wire_bytes;
    (void)wire_length;
    (void)consumed_length;
    (void)operation_result;
    (void)response_consumed_length;
}

static void test_server_runtime_rejects_mismatched_confirmed_response_invoke_id(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsPdu wire_pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 5U, 7U, 1000U, 100U, &diagnostic) == 1);
    snprintf(server_runtime.pending_request.object_reference, sizeof(server_runtime.pending_request.object_reference), "%s", "XCBR1.ST.Pos.stVal");
    snprintf(server_runtime.pending_request.attribute_reference, sizeof(server_runtime.pending_request.attribute_reference), "%s", "stVal");

    unitlab_mms_pdu_init(&wire_pdu);
    wire_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_RESPONSE;
    wire_pdu.has_invoke_id = 1;
    wire_pdu.invoke_id = 7U;
    wire_pdu.has_service = 1;
    wire_pdu.service_kind = UNITLAB_MMS_SERVICE_READ;

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result) == 0);
    assert(operation_result.ok == 0);
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_INVOKE_ID_MISMATCH);
    assert(operation_result.event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_CORRELATION_MISMATCH);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
}

static void test_server_runtime_confirmed_response_fails_after_timeout(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[128U];
    uint8_t response_payload[5U] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x00U };
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 11U, 7U, 1000U, 100U, &diagnostic) == 1);
    assert(unitlab_mms_pending_request_mark_timed_out(&server_runtime.pending_request, 1100U, &diagnostic) == 1);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_TIMED_OUT);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_TIMED_OUT);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic) == 0);
    assert(response_length == 0U);
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BAD_STATE);
}


static void test_server_runtime_apply_write_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    UnitLabMmsAssociationFrame fixture;
    uint8_t response_bytes[256];
    uint8_t response_payload[9U] = { 0x02U, 0x01U, 0x2BU, 0xA5U, 0x04U, 0x30U, 0x02U, 0x81U, 0x00U };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    {
        UnitLabMmsAssociationFrame request_fixture;
        UnitLabMmsPdu request_pdu;
        uint8_t request_payload[] = { 0x02U, 0x01U, 0x2BU, 0xA5U, 0x24U, 0x30U, 0x22U, 0xA0U, 0x1BU, 0x30U, 0x19U, 0xA0U, 0x17U, 0xA1U, 0x15U, 0x1AU, 0x05U, 'X', 'C', 'B', 'R', '1', 0x1AU, 0x0CU, 'S', 'T', '$', 'P', 'o', 's', '$', 's', 't', 'V', 'a', 'l', 0xA0U, 0x03U, 0x83U, 0x01U, 0xFFU };
        uint8_t request_encoded[256U];
        size_t request_length = 0U;
        size_t request_frame_length = 0U;

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.pdu_bytes = request_payload;
        request_pdu.pdu_length = sizeof(request_payload);
        assert(unitlab_mms_pdu_encode(&request_pdu, request_encoded, sizeof(request_encoded), &request_length, &diagnostic));
        unitlab_mms_association_frame_init(&request_fixture);
        request_fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
        request_fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
        request_fixture.presentation.payload_bytes = request_encoded;
        request_fixture.presentation.payload_length = request_length;
        request_fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
        assert(unitlab_mms_association_frame_encode(&request_fixture, wire_bytes, sizeof(wire_bytes), &request_frame_length, &diagnostic));
        wire_length = request_frame_length;
    }

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_WRITE);
    assert(server_runtime.pending_request.invoke_id == 43U);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_association_frame_init(&fixture);
    assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    {
        UnitLabMmsPdu decoded_response_pdu;
        size_t response_pdu_consumed_length = 0U;

        unitlab_mms_pdu_init(&decoded_response_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_response_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_pdu_consumed_length, &diagnostic));
        assert(response_pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response_pdu.has_invoke_id == 1);
        assert(decoded_response_pdu.invoke_id == 43U);
        assert(decoded_response_pdu.has_service == 1);
        assert(decoded_response_pdu.service_kind == UNITLAB_MMS_SERVICE_WRITE);
        assert(decoded_response_pdu.pdu_length == sizeof(response_payload));
        assert(memcmp(decoded_response_pdu.pdu_bytes, response_payload, sizeof(response_payload)) == 0);
    }
}

static int build_get_name_list_request_association_bytes(const uint8_t* request_body_bytes, size_t request_body_length, uint32_t invoke_id, uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsAssociationFrame fixture;
    UnitLabMmsPdu pdu;
    UnitLabMmsBerElement service_element;
    UnitLabMmsBerElement invoke_id_element;
    uint8_t service_bytes[128U];
    uint8_t request_payload[192U];
    uint8_t request_encoded[224U];
    size_t service_length = 0U;
    size_t invoke_id_length = 0U;
    size_t request_length = 0U;
    size_t frame_length = 0U;

    unitlab_mms_ber_element_init(&service_element);
    service_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    service_element.tag.constructed = 1;
    service_element.tag.tag_number = 1U;
    service_element.value_bytes = request_body_bytes;
    service_element.value_length = request_body_length;
    if (!unitlab_mms_ber_write(&service_element, service_bytes, sizeof(service_bytes), &service_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_ber_element_init(&invoke_id_element);
    invoke_id_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL;
    invoke_id_element.tag.constructed = 0;
    invoke_id_element.tag.tag_number = 2U;
    {
        uint8_t invoke_id_raw[5U];
        size_t invoke_id_raw_length = 0U;
        uint32_t value = invoke_id;

        do {
            invoke_id_raw[sizeof(invoke_id_raw) - 1U - invoke_id_raw_length] = (uint8_t)(value & 0xFFU);
            invoke_id_raw_length++;
            value >>= 8U;
        } while (value != 0U && invoke_id_raw_length < sizeof(invoke_id_raw));
        if (invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length] & 0x80U) {
            if (sizeof(invoke_id_raw) == invoke_id_raw_length) {
                if (diagnostic != NULL) {
                    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
                }
                return 0;
            }
            invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length - 1U] = 0x00U;
            invoke_id_raw_length++;
        }
        invoke_id_element.value_bytes = &invoke_id_raw[sizeof(invoke_id_raw) - invoke_id_raw_length];
        invoke_id_element.value_length = invoke_id_raw_length;
        if (!unitlab_mms_ber_write(&invoke_id_element, request_payload, sizeof(request_payload), &invoke_id_length, diagnostic)) {
            return 0;
        }
    }

    if (invoke_id_length + service_length > sizeof(request_payload)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
        }
        return 0;
    }
    memcpy(request_payload + invoke_id_length, service_bytes, service_length);
    request_length = invoke_id_length + service_length;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    pdu.pdu_bytes = request_payload;
    pdu.pdu_length = request_length;
    if (!unitlab_mms_pdu_encode(&pdu, request_encoded, sizeof(request_encoded), &request_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_association_frame_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = request_encoded;
    fixture.presentation.payload_length = request_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;

    if (!unitlab_mms_association_frame_encode(&fixture, buffer, buffer_length, &frame_length, diagnostic)) {
        return 0;
    }
    *encoded_length = frame_length;
    return 1;
}

static void test_server_runtime_apply_get_name_list_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[1U];
    UnitLabIedModelDataSet data_sets[2U];
    uint8_t wire_bytes[256U];
    uint8_t response_bytes[4096U];
    const uint8_t request_payload[] = {
        0x30U, 0x0CU,
        0xA0U, 0x03U, 0x02U, 0x01U, 0x02U,
        0xA1U, 0x05U, 0x81U, 0x03U, 'L', 'D', '0'
    };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsUpdates");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 1U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 2U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 61U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 2U);
    assert(server_runtime.pending_request.browse_object_scope == 1U);
    assert(strcmp(server_runtime.pending_request.browse_domain_id, "LD0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LLN0$dsEvents", strlen("LLN0$dsEvents")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"LLN0$dsUpdates", strlen("LLN0$dsUpdates")) == 1);
    }
}

static void test_server_runtime_apply_iedscout_get_name_list_request_matches_golden_capture(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    uint8_t response_bytes[4096U];
    size_t response_length = 0U;
    size_t consumed_length = 0U;
    static const uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x24U, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U, 0x61U, 0x17U, 0x30U, 0x15U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x10U, 0xA0U, 0x0EU, 0x02U, 0x01U, 0x01U, 0xA1U, 0x09U, 0xA0U, 0x03U, 0x80U, 0x01U, 0x09U, 0xA1U, 0x02U, 0x80U, 0x00U
    };
    static const uint8_t expected_response[] = {
        0x03U, 0x00U, 0x00U, 0x32U, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U, 0x61U, 0x25U, 0x30U, 0x23U, 0x02U, 0x01U, 0x03U, 0xA0U, 0x1EU, 0xA1U, 0x1CU, 0x02U, 0x01U, 0x01U, 0xA1U, 0x17U, 0xA0U, 0x12U, 0x1AU, 0x10U, 0x53U, 0x61U, 0x6DU, 0x70U, 0x6CU, 0x65U, 0x49U, 0x45U, 0x44U, 0x44U, 0x65U, 0x76U, 0x69U, 0x63U, 0x65U, 0x31U, 0x81U, 0x01U, 0x00U
    };

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "SampleIEDDevice1");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, sizeof(request_bytes), &consumed_length, &operation_result));
    assert(consumed_length == sizeof(request_bytes));
    assert(operation_result.ok == 1);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 9U);
    assert(server_runtime.pending_request.browse_object_scope == 0U);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(response_length == sizeof(expected_response));
    assert(memcmp(response_bytes, expected_response, sizeof(expected_response)) == 0);
}


static void test_server_runtime_apply_iedscout_logical_node_directory_request_class_one_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[2U];
    UnitLabIedModelSignal signals[2U];
    uint8_t wire_bytes[512U];
    uint8_t response_bytes[2048U];
    uint8_t scratch[512U];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(signals, 0, sizeof(signals));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(signals[0].logical_device_inst, sizeof(signals[0].logical_device_inst), "%s", "LD0");
    snprintf(signals[0].logical_node_name, sizeof(signals[0].logical_node_name), "%s", "XCBR1");
    snprintf(signals[0].data_object_name, sizeof(signals[0].data_object_name), "%s", "Pos");
    snprintf(signals[1].logical_device_inst, sizeof(signals[1].logical_device_inst), "%s", "LD0");
    snprintf(signals[1].logical_node_name, sizeof(signals[1].logical_node_name), "%s", "XCBR1");
    snprintf(signals[1].data_object_name, sizeof(signals[1].data_object_name), "%s", "Loc");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 2U;
    plan.logical_nodes = logical_nodes;
    plan.signal_count = 2U;
    plan.signals = signals;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(unitlab_mms_build_get_name_list_request_frame(1U, 1U, "LD0", "LLN0", 7U, scratch, sizeof(scratch), wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 1U);
    assert(server_runtime.pending_request.browse_object_scope == 1U);
    assert(strcmp(server_runtime.pending_request.browse_domain_id, "LD0") == 0);
    assert(strcmp(server_runtime.pending_request.browse_continue_after, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"NamPlt", strlen("NamPlt")) == 1);
    }
}

static void test_server_runtime_build_confirmed_error_bytes_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[4U];
    UnitLabIedModelDataSet data_sets[4U];
    uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x2AU, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U,
        0x61U, 0x1DU, 0x30U, 0x1BU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x16U, 0xA0U, 0x14U,
        0x02U, 0x01U, 0x03U, 0xA6U, 0x0FU, 0xA0U, 0x0DU, 0xA1U, 0x0BU, 0x1AU, 0x03U,
        'L', 'D', '0', 0x1AU, 0x04U, 'L', 'L', 'N', '0'
    };
    uint8_t response_bytes[4096U];
    size_t request_length = sizeof(request_bytes);
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(logical_nodes[2].logical_device_inst, sizeof(logical_nodes[2].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[2].name, sizeof(logical_nodes[2].name), "%s", "PGGIO1");
    snprintf(logical_nodes[3].logical_device_inst, sizeof(logical_nodes[3].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[3].name, sizeof(logical_nodes[3].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "XCBR1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "PGGIO1");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].logical_device_inst, sizeof(data_sets[3].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[3].logical_node_name, sizeof(data_sets[3].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 4U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(consumed_length == request_length);
    assert(operation_result.ok == 1);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;
        UnitLabMmsPdu decoded_pdu;
        size_t frame_consumed_length = 0U;
        size_t pdu_consumed_length = 0U;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &frame_consumed_length, &diagnostic));
        assert(frame_consumed_length == response_length);
        assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
        unitlab_mms_pdu_init(&decoded_pdu);
        assert(unitlab_mms_pdu_decode(&decoded_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &pdu_consumed_length, &diagnostic));
        assert(pdu_consumed_length == fixture.presentation.payload_length);
        assert(decoded_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_pdu.has_invoke_id == 1);
        assert(decoded_pdu.invoke_id == 3U);
        assert(decoded_pdu.service_kind == UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES);
        assert(decoded_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(decoded_pdu.service_tag.constructed == 1);
        assert(decoded_pdu.service_tag.tag_number == 6U);
        assert(contains_bytes(decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, (const uint8_t*)"Mod", strlen("Mod")) == 1);
        assert(contains_bytes(decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, (const uint8_t*)"Beh", strlen("Beh")) == 1);
        assert(contains_bytes(decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, (const uint8_t*)"Health", strlen("Health")) == 1);
        assert(contains_bytes(decoded_pdu.pdu_bytes, decoded_pdu.pdu_length, (const uint8_t*)"NamPlt", strlen("NamPlt")) == 1);
    }
}


static void test_server_runtime_apply_iedscout_vmd_directory_request_scope_zero_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[4U];
    UnitLabIedModelDataSet data_sets[4U];
    uint8_t response_bytes[2048U];
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(logical_nodes[2].logical_device_inst, sizeof(logical_nodes[2].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[2].name, sizeof(logical_nodes[2].name), "%s", "PGGIO1");
    snprintf(logical_nodes[3].logical_device_inst, sizeof(logical_nodes[3].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[3].name, sizeof(logical_nodes[3].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "XCBR1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "PGGIO1");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].logical_device_inst, sizeof(data_sets[3].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[3].logical_node_name, sizeof(data_sets[3].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 4U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_GET_NAME_LIST, 7U, 1U, 1000U, 100U, &diagnostic) == 1);
    server_runtime.pending_request.browse_object_class = 2U;
    server_runtime.pending_request.browse_object_scope = 0U;
    server_runtime.pending_request.browse_domain_id[0] = '\0';
    server_runtime.pending_request.browse_continue_after[0] = '\0';

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dsEvents", strlen("dsEvents")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"dsWire", strlen("dsWire")) == 1);
    }
}

static void test_server_runtime_apply_iedscout_vmd_get_variable_access_attributes_request_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[4U];
    UnitLabIedModelDataSet data_sets[4U];
    uint8_t response_bytes[2048U];
    const uint8_t request_bytes[] = {
        0x03U, 0x00U, 0x00U, 0x2AU, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U,
        0x61U, 0x1DU, 0x30U, 0x1BU, 0x02U, 0x01U, 0x03U, 0xA0U, 0x16U, 0xA0U, 0x14U,
        0x02U, 0x01U, 0x03U, 0xA6U, 0x0FU, 0xA0U, 0x0DU, 0xA1U, 0x0BU, 0x1AU, 0x03U,
        'L', 'D', '0', 0x1AU, 0x04U, 'L', 'L', 'N', '0'
    };
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    memset(data_sets, 0, sizeof(data_sets));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    snprintf(logical_nodes[1].logical_device_inst, sizeof(logical_nodes[1].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[1].name, sizeof(logical_nodes[1].name), "%s", "XCBR1");
    snprintf(logical_nodes[2].logical_device_inst, sizeof(logical_nodes[2].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[2].name, sizeof(logical_nodes[2].name), "%s", "PGGIO1");
    snprintf(logical_nodes[3].logical_device_inst, sizeof(logical_nodes[3].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[3].name, sizeof(logical_nodes[3].name), "%s", "GGIO1");
    snprintf(data_sets[0].logical_device_inst, sizeof(data_sets[0].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[0].logical_node_name, sizeof(data_sets[0].logical_node_name), "%s", "LLN0");
    snprintf(data_sets[0].name, sizeof(data_sets[0].name), "%s", "dsEvents");
    snprintf(data_sets[1].logical_device_inst, sizeof(data_sets[1].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[1].logical_node_name, sizeof(data_sets[1].logical_node_name), "%s", "XCBR1");
    snprintf(data_sets[1].name, sizeof(data_sets[1].name), "%s", "dsEvents");
    snprintf(data_sets[2].logical_device_inst, sizeof(data_sets[2].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[2].logical_node_name, sizeof(data_sets[2].logical_node_name), "%s", "PGGIO1");
    snprintf(data_sets[2].name, sizeof(data_sets[2].name), "%s", "dsEvents");
    snprintf(data_sets[3].logical_device_inst, sizeof(data_sets[3].logical_device_inst), "%s", "LD0");
    snprintf(data_sets[3].logical_node_name, sizeof(data_sets[3].logical_node_name), "%s", "GGIO1");
    snprintf(data_sets[3].name, sizeof(data_sets[3].name), "%s", "dsWire");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 4U;
    plan.logical_nodes = logical_nodes;
    plan.data_set_count = 4U;
    plan.data_sets = data_sets;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, sizeof(request_bytes), &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == sizeof(request_bytes));
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_VARIABLE_ACCESS_ATTRIBUTES);
    assert(strcmp(server_runtime.pending_request.object_reference, "LD0.LLN0") == 0);
    assert(strcmp(server_runtime.pending_request.attribute_reference, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Beh", strlen("Beh")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Health", strlen("Health")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"NamPlt", strlen("NamPlt")) == 1);
    }
}

static void test_server_runtime_apply_iedscout_logical_node_directory_request_builds_response(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabIedModelPlan plan;
    UnitLabIedModelLogicalDevice logical_devices[1U];
    UnitLabIedModelLogicalNode logical_nodes[1U];
    uint8_t wire_bytes[512U];
    uint8_t response_bytes[2048U];
    const uint8_t request_payload[] = {
        0x30U, 0x1BU,
        0x02U, 0x01U, 0x03U,
        0xA0U, 0x16U,
        0xA0U, 0x14U,
        0x02U, 0x01U, 0x03U,
        0xA6U, 0x0FU,
        0xA0U, 0x0DU,
        0xA1U, 0x0BU,
        0x1AU, 0x03U, 'L', 'D', '0',
        0x1AU, 0x04U, 'L', 'L', 'N', '0'
    };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    memset(&plan, 0, sizeof(plan));
    memset(logical_devices, 0, sizeof(logical_devices));
    memset(logical_nodes, 0, sizeof(logical_nodes));
    snprintf(logical_devices[0].inst, sizeof(logical_devices[0].inst), "%s", "LD0");
    snprintf(logical_nodes[0].logical_device_inst, sizeof(logical_nodes[0].logical_device_inst), "%s", "LD0");
    snprintf(logical_nodes[0].name, sizeof(logical_nodes[0].name), "%s", "LLN0");
    plan.logical_device_count = 1U;
    plan.logical_devices = logical_devices;
    plan.logical_node_count = 1U;
    plan.logical_nodes = logical_nodes;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);

    assert(build_get_name_list_request_association_bytes(request_payload, sizeof(request_payload), 3U, wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_GET_NAME_LIST);
    assert(server_runtime.pending_request.browse_object_class == 3U);
    assert(server_runtime.pending_request.browse_object_scope == 1U);
    assert(strcmp(server_runtime.pending_request.browse_domain_id, "LD0") == 0);
    assert(strcmp(server_runtime.pending_request.browse_continue_after, "LLN0") == 0);

    unitlab_mms_diagnostic_clear(&diagnostic);
    {
        assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    }
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    {
        UnitLabMmsAssociationFrame fixture;

        unitlab_mms_association_frame_init(&fixture);
        assert(unitlab_mms_association_frame_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == response_length);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Mod", strlen("Mod")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Beh", strlen("Beh")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"Health", strlen("Health")) == 1);
        assert(contains_bytes(fixture.presentation.payload_bytes, fixture.presentation.payload_length, (const uint8_t*)"NamPlt", strlen("NamPlt")) == 1);
    }
}

int main(void)
{
    test_server_runtime_init_captures_default_snapshot();
    test_server_runtime_apply_model_plan_sets_active_model();
    test_server_runtime_prepare_start_stop();
    test_server_runtime_apply_association_request_bytes_accepts_acse_aarq();
    test_server_runtime_build_association_response_matches_reference_capture();
    test_server_runtime_apply_association_then_confirmed_request_keeps_session_associated();
    test_server_runtime_apply_association_request_bytes_accepts_captured_iedscout_aarq();
    test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request();
    test_wire_builder_builds_confirmed_response_frame_roundtrips();
    test_server_runtime_build_confirmed_response_bytes_roundtrips();
    test_server_runtime_confirmed_response_fails_after_timeout();
    test_server_runtime_apply_reference_confirmed_request_and_build_response_roundtrips();
    test_server_runtime_apply_get_name_list_request_and_build_response_roundtrips();
    test_server_runtime_build_get_name_list_response_handles_large_directory();
    test_server_runtime_apply_iedscout_get_name_list_request_matches_golden_capture();
    test_server_runtime_apply_iedscout_logical_node_directory_request_class_one_builds_response();
    test_server_runtime_apply_iedscout_vmd_directory_request_scope_zero_builds_response();
    test_server_runtime_apply_iedscout_vmd_get_variable_access_attributes_request_builds_response();
    test_server_runtime_build_confirmed_error_bytes_roundtrips();
    test_server_runtime_apply_iedscout_logical_node_directory_request_builds_response();
    test_server_runtime_build_confirmed_response_bytes_matches_fixture_style_object_reference();
    test_server_runtime_apply_confirmed_request_and_build_response_roundtrips();
    test_server_runtime_rejects_mismatched_confirmed_response_invoke_id();
    test_server_runtime_apply_write_request_and_build_response_roundtrips();
    test_server_runtime_apply_wire_pdu_requires_running_state();
    test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_exact_frame();
    return 0;
}
