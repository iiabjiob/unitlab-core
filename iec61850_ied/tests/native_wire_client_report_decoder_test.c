#include "model/model_plan.h"
#include "server/native_wire_client.h"
#include "server/native_wire_client_session.h"
#include "server/unitlab_mms_server_runtime.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

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

    unitlab_free_ied_model_plan(&plan);
    return 0;
}
