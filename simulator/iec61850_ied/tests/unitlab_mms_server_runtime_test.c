#include <assert.h>

#include "unitlab_mms_server_runtime.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"
#include "wire/transport/unitlab_mms_transport_frame.h"
#include "wire/session/unitlab_mms_session_spdu.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

#include <string.h>
#include <stdio.h>

static const uint8_t reference_confirmed_response_bytes[] = {
    0x03U, 0x00U, 0x00U, 0x24U, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U, 0x61U, 0x17U, 0x30U, 0x15U, 0x02U,
    0x01U, 0x03U, 0xA0U, 0x10U, 0xA1U, 0x0EU, 0x02U, 0x01U, 0x01U, 0xA4U, 0x09U, 0xA1U, 0x07U, 0x87U, 0x05U, 0x08U,
    0xBFU, 0x7EU, 0x96U, 0x18U,
};

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
static int build_initiate_request_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsAcseApdu acse_apdu;
    uint8_t acse_payload[3U] = { 0x80U, 0x01U, 0x01U };
    uint8_t acse_encoded[16];
    size_t acse_length = 0U;
    size_t payload_length = 0U;

    unitlab_mms_acse_apdu_init(&acse_apdu);
    acse_apdu.kind = UNITLAB_MMS_ACSE_APDU_AARQ;
    acse_apdu.apdu_bytes = acse_payload;
    acse_apdu.apdu_length = sizeof(acse_payload);

    if (!unitlab_mms_acse_encode(&acse_apdu, acse_encoded, sizeof(acse_encoded), &acse_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = acse_encoded;
    fixture.presentation.payload_length = acse_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
    fixture.transport.cotp.eot = 1;

    payload_length = 0U;
    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, &payload_length, diagnostic)) {
        return 0;
    }
    *encoded_length = payload_length;
    return 1;
}

static int build_confirmed_request_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsPdu pdu;
    uint8_t pdu_payload[6] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x01U, 0xAAU };
    uint8_t pdu_encoded[16];
    size_t pdu_length = 0U;
    size_t payload_length = 0U;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
    pdu.pdu_bytes = pdu_payload;
    pdu.pdu_length = sizeof(pdu_payload);

    if (!unitlab_mms_pdu_encode(&pdu, pdu_encoded, sizeof(pdu_encoded), &pdu_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
    fixture.presentation.payload_bytes = pdu_encoded;
    fixture.presentation.payload_length = pdu_length;
    fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;

    payload_length = 0U;
    if (!unitlab_mms_wire_association_fixture_encode(&fixture, buffer, buffer_length, &payload_length, diagnostic)) {
        return 0;
    }
    *encoded_length = payload_length;
    return 1;
}
static int build_reference_confirmed_request_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    static const uint8_t reference_confirmed_request[] = {
        0x03U, 0x00U, 0x00U, 0x4EU, 0x02U, 0xF0U, 0x80U, 0x01U, 0x00U, 0x01U, 0x00U, 0x61U, 0x41U, 0x30U, 0x3FU, 0x02U,
        0x01U, 0x03U, 0xA0U, 0x3AU, 0xA0U, 0x38U, 0x02U, 0x01U, 0x01U, 0xA4U, 0x33U, 0xA1U, 0x31U, 0xA0U, 0x2FU, 0x30U,
        0x2DU, 0xA0U, 0x2BU, 0xA1U, 0x29U, 0x1AU, 0x11U, 0x73U, 0x69U, 0x6DU, 0x70U, 0x6CU, 0x65U, 0x49U, 0x4FU, 0x47U,
        0x65U, 0x6EU, 0x65U, 0x72U, 0x69U, 0x63U, 0x49U, 0x4FU, 0x1AU, 0x14U, 0x47U, 0x47U, 0x49U, 0x4FU, 0x31U, 0x24U,
        0x4DU, 0x58U, 0x24U, 0x41U, 0x6EU, 0x49U, 0x6EU, 0x31U, 0x24U, 0x6DU, 0x61U, 0x67U, 0x24U, 0x66U,
    };

    if (buffer_length < sizeof(reference_confirmed_request)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            diagnostic->message[0] = '\0';
        }
        return 0;
    }
    memcpy(buffer, reference_confirmed_request, sizeof(reference_confirmed_request));
    *encoded_length = sizeof(reference_confirmed_request);
    if (diagnostic != NULL) {
        diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_OK;
        diagnostic->message[0] = '\0';
    }
    return 1;
}


static void test_server_runtime_apply_association_request_bytes_accepts_acse_aarq(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
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

static void test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
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
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsPdu wire_pdu = make_information_report_pdu();

    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_operation_result_init(&operation_result);

    assert(!unitlab_mms_server_runtime_apply_wire_pdu(&server_runtime, &wire_pdu, &operation_result));
    assert(operation_result.diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_BAD_STATE);

    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };

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
    UnitLabMmsWireAssociationFixture fixture;
    uint8_t response_bytes[256];
    uint8_t response_payload[6] = { 0x02U, 0x01U, 0x29U, 0xA4U, 0x01U, 0xAAU };
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;

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
        UnitLabMmsPdu decoded_response;
        size_t transport_consumed_length = 0U;
        size_t session_consumed_length = 0U;
        size_t response_consumed_length = 0U;

        unitlab_mms_transport_frame_init(&transport_frame);
        assert(unitlab_mms_transport_frame_decode(&transport_frame, response_bytes, encoded_length, &transport_consumed_length, &diagnostic));
        assert(transport_consumed_length == encoded_length);
        unitlab_mms_session_spdu_init(&session_spdu);
        assert(unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic));
        assert(session_consumed_length == transport_frame.cotp.user_data_length);
        unitlab_mms_pdu_init(&decoded_response);
        assert(unitlab_mms_pdu_decode(&decoded_response, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == session_spdu.raw_parameter_length);
        assert(decoded_response.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response.has_invoke_id == 1);
        assert(decoded_response.invoke_id == 41U);
        assert(decoded_response.has_service == 1);
        assert(decoded_response.service_kind == UNITLAB_MMS_SERVICE_READ);
    }
}

static void test_server_runtime_build_confirmed_response_bytes_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t response_bytes[256];
    uint8_t response_payload[6] = { 0x02U, 0x01U, 0x29U, 0xA4U, 0x01U, 0xAAU };
    size_t encoded_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsWireAssociationFixture fixture;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));

    unitlab_mms_pending_request_init(&server_runtime.pending_request);
    assert(unitlab_mms_pending_request_start(&server_runtime.pending_request, UNITLAB_MMS_REQUEST_READ, 41U, 7U, 1000U, 100U, &diagnostic) == 1);

    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &encoded_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);
    assert(server_runtime.transport.response_bytes == response_bytes);
    assert(server_runtime.transport.response_length == encoded_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH);

    {
        UnitLabMmsTransportFrame transport_frame;
        UnitLabMmsSessionSpdu session_spdu;
        UnitLabMmsPdu decoded_response;
        size_t transport_consumed_length = 0U;
        size_t session_consumed_length = 0U;
        size_t response_consumed_length = 0U;

        unitlab_mms_transport_frame_init(&transport_frame);
        assert(unitlab_mms_transport_frame_decode(&transport_frame, response_bytes, encoded_length, &transport_consumed_length, &diagnostic));
        assert(transport_consumed_length == encoded_length);
        unitlab_mms_session_spdu_init(&session_spdu);
        assert(unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic));
        assert(session_consumed_length == transport_frame.cotp.user_data_length);
        unitlab_mms_pdu_init(&decoded_response);
        assert(unitlab_mms_pdu_decode(&decoded_response, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == session_spdu.raw_parameter_length);
        assert(decoded_response.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response.has_invoke_id == 1);
        assert(decoded_response.invoke_id == 41U);
        assert(decoded_response.has_service == 1);
        assert(decoded_response.service_kind == UNITLAB_MMS_SERVICE_READ);
    }
}

static void test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_tail(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t tail_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_request_general_interrogation(&server_runtime, &diagnostic));
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING);

    assert(build_information_report_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    wire_bytes[wire_length++] = 0xAAU;
    wire_bytes[wire_length++] = 0x55U;

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length < wire_length);
    tail_length = wire_length - consumed_length;
    assert(tail_length == 2U);
    assert(server_runtime.transport.request_bytes == wire_bytes);
    assert(server_runtime.transport.request_length == consumed_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_BIND_REQUEST);
    assert(server_runtime.report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING);
    assert(server_runtime.state == UNITLAB_MMS_SERVER_RUNTIME_RUNNING);
}

static void test_server_runtime_apply_reference_confirmed_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsPdu response_pdu;
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    assert(build_reference_confirmed_request_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_wire_association_fixture_init(&fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&fixture, wire_bytes, wire_length, &consumed_length, &diagnostic));

    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 3U);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);
    fprintf(stderr, "runtime confirmed-response length=%zu\n", response_length);
    fprintf(stderr, "runtime confirmed-response bytes:");
    for (size_t i = 0U; i < response_length; i++) {
        fprintf(stderr, " %02X", response_bytes[i]);
    }
    fprintf(stderr, "\n");
    assert(response_length == sizeof(reference_confirmed_response_bytes));
    assert(memcmp(response_bytes, reference_confirmed_response_bytes, sizeof(reference_confirmed_response_bytes)) == 0);
    assert(server_runtime.transport.response_bytes == response_bytes);
    assert(server_runtime.transport.response_length == response_length);
    assert(server_runtime.transport.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_TRANSPORT_SET_RESPONSE_LENGTH);

    {
        UnitLabMmsTransportFrame transport_frame;
        UnitLabMmsSessionSpdu session_spdu;
        size_t transport_consumed_length = 0U;
        size_t session_consumed_length = 0U;
        size_t decoded_length = 0U;

        unitlab_mms_transport_frame_init(&transport_frame);
        assert(unitlab_mms_transport_frame_decode(&transport_frame, response_bytes, response_length, &transport_consumed_length, &diagnostic));
        assert(transport_consumed_length == response_length);
        unitlab_mms_session_spdu_init(&session_spdu);
        assert(unitlab_mms_session_spdu_decode(&session_spdu, transport_frame.cotp.user_data, transport_frame.cotp.user_data_length, &session_consumed_length, &diagnostic));
        assert(session_consumed_length == transport_frame.cotp.user_data_length);

        unitlab_mms_pdu_init(&response_pdu);
        assert(unitlab_mms_pdu_decode(&response_pdu, session_spdu.raw_parameter_bytes, session_spdu.raw_parameter_length, &decoded_length, &diagnostic) == 1);
        assert(decoded_length == session_spdu.raw_parameter_length);
        assert(response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(response_pdu.has_invoke_id == 1);
        assert(response_pdu.invoke_id == 3U);
        assert(response_pdu.has_service == 1);
        assert(response_pdu.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(response_pdu.service_length == 16U);
        assert(response_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
        assert(response_pdu.service_tag.tag_number == 0U);
    }
}

static void test_server_runtime_apply_confirmed_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabMmsWireAssociationFixture fixture;
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    uint8_t response_payload[5U] = { 0x02U, 0x01U, 0x05U, 0xA4U, 0x00U };
    size_t wire_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t response_consumed_length = 0U;

    unitlab_mms_server_runtime_init(&server_runtime);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));

    assert(build_confirmed_request_association_bytes(wire_bytes, sizeof(wire_bytes), &wire_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, wire_bytes, wire_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(consumed_length == wire_length);
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_ACTIVE);
    assert(server_runtime.pending_request.kind == UNITLAB_MMS_REQUEST_READ);
    assert(server_runtime.pending_request.invoke_id == 5U);
    assert(server_runtime.pending_request.last_event.kind == UNITLAB_MMS_RUNTIME_EVENT_REQUEST_STARTED);

    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, response_payload, sizeof(response_payload), response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(response_length > 0U);

    unitlab_mms_wire_association_fixture_init(&fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(fixture.presentation.payload_length > 0U);
    {
        UnitLabMmsPdu decoded_response;
        size_t decoded_length = 0U;

        unitlab_mms_pdu_init(&decoded_response);
        assert(unitlab_mms_pdu_decode(&decoded_response, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &decoded_length, &diagnostic));
        assert(decoded_length == fixture.presentation.payload_length);
        assert(decoded_response.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
        assert(decoded_response.has_invoke_id == 1);
        assert(decoded_response.invoke_id == 5U);
        assert(decoded_response.has_service == 1);
        assert(decoded_response.service_kind == UNITLAB_MMS_SERVICE_READ);
        assert(decoded_response.service_length == 0U);
    }

    assert(unitlab_mms_pending_request_complete(&server_runtime.pending_request, 1234U, &diagnostic));
    assert(server_runtime.pending_request.state == UNITLAB_MMS_PENDING_REQUEST_COMPLETED);
}

static void test_server_runtime_apply_write_request_and_build_response_roundtrips(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsOperationResult operation_result;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabIedServerConfig config = {
        .bind_address = "127.0.0.1",
        .port = 102,
    };
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsPdu response_pdu;
    uint8_t wire_bytes[256];
    uint8_t response_bytes[256];
    uint8_t response_payload[5U] = { 0x02U, 0x01U, 0x2BU, 0xA5U, 0x00U };
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
        UnitLabMmsWireAssociationFixture request_fixture;
        UnitLabMmsPdu request_pdu;
        uint8_t request_payload[5U] = { 0x02U, 0x01U, 0x2BU, 0xA5U, 0x00U };
        uint8_t request_encoded[16U];
        size_t request_length = 0U;
        size_t request_frame_length = 0U;

        unitlab_mms_pdu_init(&request_pdu);
        request_pdu.kind = UNITLAB_MMS_PDU_CONFIRMED_REQUEST;
        request_pdu.pdu_bytes = request_payload;
        request_pdu.pdu_length = sizeof(request_payload);
        assert(unitlab_mms_pdu_encode(&request_pdu, request_encoded, sizeof(request_encoded), &request_length, &diagnostic));
        unitlab_mms_wire_association_fixture_init(&request_fixture);
        request_fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
        request_fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED;
        request_fixture.presentation.payload_bytes = request_encoded;
        request_fixture.presentation.payload_length = request_length;
        request_fixture.transport.cotp.kind = UNITLAB_MMS_COTP_TPDU_DT;
        assert(unitlab_mms_wire_association_fixture_encode(&request_fixture, wire_bytes, sizeof(wire_bytes), &request_frame_length, &diagnostic));
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

    unitlab_mms_wire_association_fixture_init(&fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&fixture, response_bytes, response_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == response_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_FULLY_ENCODED);
    assert(fixture.presentation.payload_length > 0U);
    unitlab_mms_pdu_init(&response_pdu);
    assert(unitlab_mms_pdu_decode(&response_pdu, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_consumed_length, &diagnostic));
    assert(response_consumed_length == fixture.presentation.payload_length);
    assert(response_pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE);
    assert(response_pdu.has_invoke_id == 1);
    assert(response_pdu.invoke_id == 43U);
    assert(response_pdu.has_service == 1);
    assert(response_pdu.service_kind == UNITLAB_MMS_SERVICE_WRITE);
    assert(response_pdu.service_length == 0U);
    assert(response_pdu.service_tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC);
    assert(response_pdu.service_tag.tag_number == 5U);
}

int main(void)
{
    test_server_runtime_init_captures_default_snapshot();
    test_server_runtime_prepare_start_stop();
    test_server_runtime_apply_association_request_bytes_accepts_acse_aarq();
    test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request();
    test_wire_builder_builds_confirmed_response_frame_roundtrips();
    test_server_runtime_build_confirmed_response_bytes_roundtrips();
    test_server_runtime_apply_reference_confirmed_request_and_build_response_roundtrips();
    test_server_runtime_apply_confirmed_request_and_build_response_roundtrips();
    test_server_runtime_apply_write_request_and_build_response_roundtrips();
    test_server_runtime_apply_wire_pdu_requires_running_state();
    test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_tail();
    return 0;
}
