#include "model/model_plan.h"
#include "server/native_wire_client.h"
#include "server/native_wire_client_session.h"
#include "server/unitlab_mms_server_runtime.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"
#include "wire/orchestration/unitlab_mms_wire_builder_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int build_report_plan(UnitLabIedModelPlan* plan)
{
    UnitLabIedFixtureSignal signals[2U] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 1U,
            .reference = "LD0/PGGIO1.Ind1.stVal[ST]",
            .kind = "FCDA",
            .component = "stVal",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1U] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 2U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1U] = {
        {
            .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
            .logical_device_inst = "LD0",
            .logical_node_name = "LLN0",
            .report_control_name = "brcbEvents",
            .report_kind = "buffered",
            .rpt_id = "IED1LD0/LLN0.BR.Events",
            .data_set_ref = "IED1/AP1/LD0/LLN0.dsEvents",
            .conf_rev = "7",
            .indexed_known = 1,
            .indexed = 0,
            .buffer_time_ms_known = 1,
            .buffer_time_ms = 100,
            .integrity_period_ms_known = 1,
            .integrity_period_ms = 1000,
        },
    };
    UnitLabIedFixtureModel fixture;
    char error[256U];

    reports[0].optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    reports[0].optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };

    memset(&fixture, 0, sizeof(fixture));
    fixture.device_count = 1U;
    snprintf(fixture.ied_name, sizeof(fixture.ied_name), "%s", "IED1");
    snprintf(fixture.access_point_name, sizeof(fixture.access_point_name), "%s", "AP1");
    fixture.data_set_count = 1U;
    fixture.data_sets = data_sets;
    fixture.report_count = 1U;
    fixture.reports = reports;
    fixture.signal_count = 2U;
    return unitlab_build_ied_model_plan(&fixture, plan, error, sizeof(error));
}

static void append_report_leaf_refs(UnitLabNativeClientSessionState* session)
{
    assert(unitlab_native_client_session_append_leaf_ref(session, "IED1LD0/XCBR1$ST$Pos$stVal") != NULL);
    assert(unitlab_native_client_session_append_leaf_ref(session, "IED1LD0/PGGIO1$ST$Ind1$stVal") != NULL);
}

static void prepare_discovered_report_model(UnitLabNativeClientSessionState* session)
{
    UnitLabNativeDiscoveredDataSet* data_set;

    data_set = unitlab_native_client_session_append_data_set(session, "IED1LD0/LLN0$dsEvents");
    assert(data_set != NULL);
    assert(unitlab_native_client_session_append_data_set_member(session, data_set, "IED1LD0/XCBR1$ST$Pos$stVal") == 1);
    assert(unitlab_native_client_session_append_data_set_member(session, data_set, "IED1LD0/PGGIO1$ST$Ind1$stVal") == 1);
    append_report_leaf_refs(session);
}

static void prepare_mismatched_discovered_report_model(UnitLabNativeClientSessionState* session)
{
    UnitLabNativeDiscoveredDataSet* data_set;

    data_set = unitlab_native_client_session_append_data_set(session, "IED1LD0/LLN0$dsEvents");
    assert(data_set != NULL);
    assert(unitlab_native_client_session_append_data_set_member(session, data_set, "IED1LD0/LLN0$ST$Health$stVal") == 1);
    append_report_leaf_refs(session);
}


static int append_test_ber(uint8_t* buffer, size_t buffer_length, size_t* offset, UnitLabMmsBerTagClass tag_class, int constructed, uint32_t tag_number, const uint8_t* value, size_t value_length, UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_length = 0U;

    if (buffer == NULL || offset == NULL || *offset > buffer_length) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(tag_class, constructed, tag_number, value, value_length, &buffer[*offset], buffer_length - *offset, &encoded_length, diagnostic)) {
        return 0;
    }
    *offset += encoded_length;
    return 1;
}

static int build_synthetic_report_frame(size_t value_count, size_t reason_count, uint8_t* buffer, size_t buffer_length, size_t* encoded_length)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsPdu report_pdu;
    uint8_t scratch[4096U];
    uint8_t variable_list_name[32U];
    uint8_t service_content[512U];
    uint8_t report_values[512U];
    uint8_t service_bytes[768U];
    size_t variable_list_name_length = 0U;
    size_t service_content_length = 0U;
    size_t report_values_length = 0U;
    size_t service_length = 0U;
    const uint8_t opt_flds[3U] = { 0x06U, 0x7fU, 0x80U };
    const uint8_t sq_num[1U] = { 0x00U };
    const uint8_t time_of_entry[6U] = { 0x02U, 0xfcU, 0x40U, 0x70U, 0x3cU, 0x94U };
    const uint8_t bool_false[1U] = { 0x00U };
    const uint8_t entry_id[8U] = { 0x00U, 0x00U, 0x01U, 0x9eU, 0xd5U, 0xdcU, 0xecU, 0x70U };
    const uint8_t conf_rev[1U] = { 0x07U };
    const uint8_t inclusion[2U] = { 0x06U, 0xc0U };
    const uint8_t zero_value[1U] = { 0x00U };
    const uint8_t reason_gi[2U] = { 0x02U, 0x04U };

    unitlab_mms_diagnostic_clear(&diagnostic);
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        return 0;
    }

    if (!append_test_ber(variable_list_name, sizeof(variable_list_name), &variable_list_name_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 0U, (const uint8_t*)"RPT", 3U, &diagnostic)
        || !append_test_ber(service_content, sizeof(service_content), &service_content_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 1U, variable_list_name, variable_list_name_length, &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)"IED1LD0/LLN0.BR.Events", strlen("IED1LD0/LLN0.BR.Events"), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 4U, opt_flds, sizeof(opt_flds), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 6U, sq_num, sizeof(sq_num), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 12U, time_of_entry, sizeof(time_of_entry), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)"IED1LD0/LLN0$dsEvents", strlen("IED1LD0/LLN0$dsEvents"), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 3U, bool_false, sizeof(bool_false), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 9U, entry_id, sizeof(entry_id), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 6U, conf_rev, sizeof(conf_rev), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 4U, inclusion, sizeof(inclusion), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)"IED1LD0/XCBR1$ST$Pos$stVal", strlen("IED1LD0/XCBR1$ST$Pos$stVal"), &diagnostic)
        || !append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 10U, (const uint8_t*)"IED1LD0/PGGIO1$ST$Ind1$stVal", strlen("IED1LD0/PGGIO1$ST$Ind1$stVal"), &diagnostic)) {
        return 0;
    }

    for (size_t index = 0U; index < value_count; index++) {
        if (!append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 5U, zero_value, sizeof(zero_value), &diagnostic)) {
            return 0;
        }
    }
    for (size_t index = 0U; index < reason_count; index++) {
        if (!append_test_ber(report_values, sizeof(report_values), &report_values_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 0, 4U, reason_gi, sizeof(reason_gi), &diagnostic)) {
            return 0;
        }
    }
    if (!append_test_ber(service_content, sizeof(service_content), &service_content_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U, report_values, report_values_length, &diagnostic)
        || !append_test_ber(service_bytes, sizeof(service_bytes), &service_length, UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC, 1, 0U, service_content, service_content_length, &diagnostic)) {
        return 0;
    }

    unitlab_mms_pdu_init(&report_pdu);
    report_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    report_pdu.has_service = 1;
    report_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    report_pdu.pdu_bytes = service_bytes;
    report_pdu.pdu_length = service_length;
    return unitlab_mms_build_wire_frame_from_pdu(&report_pdu, scratch, sizeof(scratch), buffer, buffer_length, encoded_length, &diagnostic);
}

static void assert_synthetic_report_counts(size_t value_count, size_t reason_count, size_t missing_values, size_t extra_values, size_t missing_reasons, size_t extra_reasons)
{
    UnitLabNativeClientSessionState session;
    uint8_t report_bytes[4096U];
    size_t report_length = 0U;

    assert(build_synthetic_report_frame(value_count, reason_count, report_bytes, sizeof(report_bytes), &report_length) == 1);
    memset(&session, 0, sizeof(session));
    prepare_discovered_report_model(&session);
    assert(unitlab_native_wire_client_decode_frame_summary(&session, report_bytes, report_length) == 1);
    assert(session.discovered_model.last_report_data_ref_count == 2U);
    assert(session.last_report_entry_count == 2U);
    assert(session.discovered_model.last_report_value_count == value_count);
    assert(session.discovered_model.last_report_reason_count == reason_count);
    assert(session.discovered_model.last_report_missing_value_count == missing_values);
    assert(session.discovered_model.last_report_extra_value_count == extra_values);
    assert(session.discovered_model.last_report_missing_reason_count == missing_reasons);
    assert(session.discovered_model.last_report_extra_reason_count == extra_reasons);
    assert(session.discovered_model.last_report_dataset_mismatch_count == 0U);
    unitlab_native_client_session_reset(&session);
}

int main(void)
{
    UnitLabMmsServerRuntime server_runtime;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsOperationResult operation_result;
    UnitLabIedServerConfig config = { .bind_address = "127.0.0.1", .port = 102 };
    UnitLabIedModelPlan plan;
    UnitLabNativeClientSessionState session;
    uint8_t scratch[512U];
    uint8_t request_bytes[512U];
    uint8_t response_bytes[1024U];
    uint8_t report_bytes[4096U];
    uint8_t value_byte = 0x01U;
    UnitLabMmsBerElement data_element;
    size_t request_length = 0U;
    size_t consumed_length = 0U;
    size_t response_length = 0U;
    size_t report_length = 0U;

    assert(build_report_plan(&plan) == 1);
    unitlab_mms_server_runtime_init(&server_runtime);
    unitlab_mms_diagnostic_clear(&diagnostic);
    assert(unitlab_mms_server_runtime_apply_model_plan(&server_runtime, &plan) == 1);
    assert(unitlab_mms_server_runtime_prepare(&server_runtime, &config, &diagnostic));
    assert(unitlab_mms_server_runtime_start(&server_runtime, &diagnostic));
    assert(unitlab_mms_session_begin_association(&server_runtime.session, &diagnostic));
    assert(unitlab_mms_session_complete_association(&server_runtime.session, 1U, &diagnostic));
    assert(unitlab_mms_server_runtime_reserve_report_control(&server_runtime, &diagnostic));
    assert(unitlab_mms_server_runtime_enable_report_control(&server_runtime, &diagnostic));
    server_runtime.brcb_rpt_ena = 1U;

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &value_byte;
    data_element.value_length = 1U;
    assert(unitlab_mms_build_write_request_frame("IED1LD0", "LLN0$BR$brcbEvents$GI", &data_element, 23U, scratch, sizeof(scratch), request_bytes, sizeof(request_bytes), &request_length, &diagnostic));
    unitlab_mms_operation_result_init(&operation_result);
    assert(unitlab_mms_server_runtime_apply_incoming_bytes(&server_runtime, request_bytes, request_length, &consumed_length, &operation_result));
    assert(operation_result.ok == 1);
    assert(unitlab_mms_server_runtime_build_confirmed_response_bytes(&server_runtime, NULL, 0U, response_bytes, sizeof(response_bytes), &response_length, &diagnostic));
    assert(unitlab_mms_server_runtime_build_pending_gi_report_bytes(&server_runtime, report_bytes, sizeof(report_bytes), &report_length, &diagnostic));

    memset(&session, 0, sizeof(session));
    prepare_discovered_report_model(&session);
    assert(unitlab_native_wire_client_decode_frame_summary(&session, report_bytes, report_length) == 1);
    assert(session.subscription_model.last_report_received == 1);
    assert(session.discovered_model.data_set_count == 1U);
    assert(session.discovered_model.data_set_member_count == 2U);
    assert(session.discovered_model.last_report_data_ref_count == 2U);
    assert(session.discovered_model.last_report_value_count == 2U);
    assert(session.discovered_model.last_report_reason_count == 2U);
    assert(session.discovered_model.last_report_matched_data_ref_count == 2U);
    assert(session.discovered_model.last_report_dataset_mismatch_count == 0U);
    assert(session.discovered_model.last_report_missing_value_count == 0U);
    assert(session.discovered_model.last_report_extra_value_count == 0U);
    assert(session.discovered_model.last_report_missing_reason_count == 0U);
    assert(session.discovered_model.last_report_extra_reason_count == 0U);
    assert(session.discovered_model.last_report_unsupported_value_count == 0U);
    assert(session.last_report_entry_count == 2U);
    assert(strcmp(session.last_report_entries[0].data_reference, "IED1LD0/XCBR1$ST$Pos$stVal") == 0);
    assert(session.last_report_entries[0].dataset_match == 1);
    assert(session.last_report_entries[0].reason_flags & UNITLAB_NATIVE_REPORT_REASON_GENERAL_INTERROGATION);

    unitlab_native_client_session_reset(&session);

    memset(&session, 0, sizeof(session));
    prepare_mismatched_discovered_report_model(&session);
    assert(unitlab_native_wire_client_decode_frame_summary(&session, report_bytes, report_length) == 1);
    assert(session.subscription_model.last_report_received == 1);
    assert(session.discovered_model.data_set_count == 1U);
    assert(session.discovered_model.data_set_member_count == 1U);
    assert(session.discovered_model.last_report_data_ref_count == 2U);
    assert(session.discovered_model.last_report_value_count == 2U);
    assert(session.discovered_model.last_report_reason_count == 2U);
    assert(session.discovered_model.last_report_matched_data_ref_count == 0U);
    assert(session.discovered_model.last_report_dataset_mismatch_count == 2U);
    assert(session.discovered_model.last_report_missing_value_count == 0U);
    assert(session.discovered_model.last_report_extra_value_count == 0U);
    assert(session.discovered_model.last_report_missing_reason_count == 0U);
    assert(session.discovered_model.last_report_extra_reason_count == 0U);
    assert(session.discovered_model.last_report_unsupported_value_count == 0U);
    assert(session.last_report_entry_count == 2U);
    assert(session.last_report_entries[0].dataset_match == 0);
    assert(session.last_report_entries[1].dataset_match == 0);
    unitlab_native_client_session_reset(&session);

    assert_synthetic_report_counts(1U, 2U, 1U, 0U, 0U, 0U);
    assert_synthetic_report_counts(3U, 2U, 0U, 1U, 0U, 0U);
    assert_synthetic_report_counts(2U, 1U, 0U, 0U, 1U, 0U);
    assert_synthetic_report_counts(2U, 3U, 0U, 0U, 0U, 1U);

    unitlab_free_ied_model_plan(&plan);
    return 0;
}
