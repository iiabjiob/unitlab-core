#include <assert.h>

#include "unitlab_mms_server_runtime.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/transport/unitlab_mms_wire_association_fixture.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

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
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsPdu pdu;
    uint8_t pdu_raw_payload[2];
    uint8_t pdu_encoded[16];
    size_t pdu_length = 0U;
    size_t payload_length = 0U;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    pdu.has_service = 1;
    pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    pdu_raw_payload[0] = 0x80U;
    pdu_raw_payload[1] = 0x00U;
    pdu.pdu_bytes = pdu_raw_payload;
    pdu.pdu_length = sizeof(pdu_raw_payload);

    if (!unitlab_mms_pdu_encode(&pdu, pdu_encoded, sizeof(pdu_encoded), &pdu_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
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
static int build_initiate_request_association_bytes(uint8_t* buffer, size_t buffer_length, size_t* encoded_length, UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsWireAssociationFixture fixture;
    UnitLabMmsPdu pdu;
    uint8_t pdu_encoded[16];
    size_t pdu_length = 0U;
    size_t payload_length = 0U;

    unitlab_mms_pdu_init(&pdu);
    pdu.kind = UNITLAB_MMS_PDU_INITIATE_REQUEST;
    pdu.pdu_bytes = NULL;
    pdu.pdu_length = 0U;

    if (!unitlab_mms_pdu_encode(&pdu, pdu_encoded, sizeof(pdu_encoded), &pdu_length, diagnostic)) {
        return 0;
    }

    unitlab_mms_wire_association_fixture_init(&fixture);
    fixture.session.kind = UNITLAB_MMS_SESSION_SPDU_DATA_TRANSFER;
    fixture.presentation.kind = UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED;
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

static void test_server_runtime_apply_association_request_bytes_accepts_initiate_request(void)
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

    assert(unitlab_mms_build_confirmed_response_frame(&response_pdu, response_bytes, sizeof(response_bytes), &encoded_length, &diagnostic));
    assert(diagnostic.code == UNITLAB_MMS_DIAGNOSTIC_OK);
    assert(encoded_length > 0U);

    unitlab_mms_wire_association_fixture_init(&fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&fixture, response_bytes, encoded_length, &consumed_length, &diagnostic));
    assert(consumed_length == encoded_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(fixture.presentation.payload_length > 0U);
    {
        UnitLabMmsPdu decoded_response;
        size_t response_consumed_length = 0U;

        unitlab_mms_pdu_init(&decoded_response);
        assert(unitlab_mms_pdu_decode(&decoded_response, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == fixture.presentation.payload_length);
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

    unitlab_mms_wire_association_fixture_init(&fixture);
    assert(unitlab_mms_wire_association_fixture_decode(&fixture, response_bytes, encoded_length, &consumed_length, &diagnostic));
    assert(consumed_length == encoded_length);
    assert(fixture.presentation.kind == UNITLAB_MMS_PRESENTATION_APDU_SIMPLY_ENCODED);
    assert(fixture.presentation.payload_length > 0U);
    {
        UnitLabMmsPdu decoded_response;
        size_t response_consumed_length = 0U;

        unitlab_mms_pdu_init(&decoded_response);
        assert(unitlab_mms_pdu_decode(&decoded_response, fixture.presentation.payload_bytes, fixture.presentation.payload_length, &response_consumed_length, &diagnostic));
        assert(response_consumed_length == fixture.presentation.payload_length);
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

int main(void)
{
    test_server_runtime_init_captures_default_snapshot();
    test_server_runtime_prepare_start_stop();
    test_server_runtime_apply_association_request_bytes_accepts_initiate_request();
    test_server_runtime_apply_association_request_bytes_rejects_non_initiate_request();
    test_wire_builder_builds_confirmed_response_frame_roundtrips();
    test_server_runtime_build_confirmed_response_bytes_roundtrips();
    test_server_runtime_apply_wire_pdu_requires_running_state();
    test_server_runtime_apply_incoming_bytes_roundtrips_and_consumes_tail();
    return 0;
}
