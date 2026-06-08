#include "server/unitlab_mms_server_runtime_internal.h"

#include <string.h>

/* Write service applies confirmed Write side effects and builds WriteResponse. */

static uint32_t server_runtime_decode_write_unsigned_bytes(const uint8_t* value_bytes, size_t value_length)
{
    uint32_t value = 0U;

    if (value_bytes == NULL || value_length == 0U) {
        return 0U;
    }
    for (size_t index = 0U; index < value_length && index < sizeof(uint32_t); index++) {
        value = (value << 8U) | (uint32_t)value_bytes[index];
    }
    return value;
}


static int server_runtime_write_reference_matches_report_control_field(const char* object_reference, const char* field_name)
{
    if (object_reference == NULL || field_name == NULL) {
        return 0;
    }
    return server_runtime_object_reference_matches_report_control_field(object_reference, field_name);
}



static void server_runtime_normalize_reference_key(const char* reference, char* buffer, size_t buffer_length)
{
    size_t offset = 0U;

    if (buffer == NULL || buffer_length == 0U) {
        return;
    }
    buffer[0] = '\0';
    if (reference == NULL) {
        return;
    }
    for (size_t index = 0U; reference[index] != '\0' && offset + 1U < buffer_length; index++) {
        char ch = reference[index];
        if (ch == '/' || ch == '$') {
            ch = '.';
        }
        buffer[offset++] = ch;
    }
    buffer[offset] = '\0';
}

static int server_runtime_reference_matches_signal(const char* object_reference, const UnitLabIedModelSignal* signal)
{
    char object_key[256U];
    char signal_key[256U];

    if (object_reference == NULL || signal == NULL) {
        return 0;
    }
    server_runtime_normalize_reference_key(object_reference, object_key, sizeof(object_key));
    if (signal->object_reference[0] != '\0') {
        server_runtime_normalize_reference_key(signal->object_reference, signal_key, sizeof(signal_key));
        if (strcmp(object_key, signal_key) == 0 || strstr(object_key, signal_key) != NULL) {
            return 1;
        }
    }
    if (signal->data_set_entry_variable[0] != '\0') {
        server_runtime_normalize_reference_key(signal->data_set_entry_variable, signal_key, sizeof(signal_key));
        if (strcmp(object_key, signal_key) == 0 || strstr(object_key, signal_key) != NULL) {
            return 1;
        }
    }
    return 0;
}


static int server_runtime_encode_written_signal_value(
    const UnitLabIedModelSignal* signal,
    const uint8_t* value_bytes,
    size_t value_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint32_t tag_number = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (signal == NULL || value_bytes == NULL || value_length == 0U || buffer == NULL || encoded_length == NULL) {
        return 0;
    }
    switch (signal->initial_value_kind) {
        case UNITLAB_IED_FIXTURE_VALUE_BOOLEAN:
            tag_number = 3U;
            break;
        case UNITLAB_IED_FIXTURE_VALUE_INTEGER:
            tag_number = 5U;
            break;
        case UNITLAB_IED_FIXTURE_VALUE_STRING:
            tag_number = 10U;
            break;
        default:
            return 0;
    }
    return server_runtime_encode_ber_element(
        UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
        0,
        tag_number,
        value_bytes,
        value_length,
        buffer,
        buffer_length,
        encoded_length,
        diagnostic);
}

int unitlab_mms_server_runtime_queue_data_change_report_value(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    const uint8_t* value_bytes,
    size_t value_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const UnitLabIedModelReportControl* report;
    const UnitLabIedModelDataSet* data_set;

    if (server_runtime == NULL || object_reference == NULL || value_bytes == NULL || value_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Data-change report queue requires runtime, object reference, and value bytes.");
        return 0;
    }
    if (server_runtime->brcb_rpt_ena == 0U || server_runtime->model_plan == NULL || server_runtime->model_plan->report_count == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Data-change report queue requires enabled BRCB and model-backed report control.");
        return 0;
    }
    report = &server_runtime->model_plan->reports[0];
    if (report->data_set_index >= server_runtime->model_plan->data_set_count || server_runtime->model_plan->data_sets == NULL || server_runtime->model_plan->signals == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Data-change report queue could not resolve the active report DataSet.");
        return 0;
    }
    data_set = &server_runtime->model_plan->data_sets[report->data_set_index];
    for (size_t index = 0U; index < data_set->member_count; index++) {
        size_t signal_index = data_set->first_signal_index + index;
        if (signal_index >= server_runtime->model_plan->signal_count) {
            break;
        }
        if (server_runtime_reference_matches_signal(object_reference, &server_runtime->model_plan->signals[signal_index])) {
            size_t encoded_value_length = 0U;
            if (!server_runtime_encode_written_signal_value(
                    &server_runtime->model_plan->signals[signal_index],
                    value_bytes,
                    value_length,
                    server_runtime->pending_report_value,
                    sizeof(server_runtime->pending_report_value),
                    &encoded_value_length,
                    diagnostic)) {
                return 0;
            }
            server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE;
            server_runtime->pending_gi_report = 1U;
            server_runtime->pending_report_member_index = index;
            server_runtime->pending_report_value_length = encoded_value_length;
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
            return 1;
        }
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Data-change report queue object is not a member of the active report DataSet.");
    return 0;
}

static int server_runtime_apply_report_control_write(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
    uint32_t value = 0U;

    if (server_runtime == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Server runtime is required for report-control write.");
        return 0;
    }
    if (server_runtime->pending_request.kind != UNITLAB_MMS_REQUEST_WRITE) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
        return 1;
    }

    {
        size_t write_count = server_runtime->pending_request.write_object_reference_count;
        if (write_count == 0U) {
            write_count = 1U;
        }
        for (size_t index = 0U; index < write_count; index++) {
            const char* object_reference = server_runtime->pending_request.object_reference;
            const uint8_t* value_bytes = server_runtime->pending_request.write_value;
            size_t value_length = server_runtime->pending_request.write_value_length;

            if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_object_references[index][0] != '\0') {
                object_reference = server_runtime->pending_request.write_object_references[index];
            }
            if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_value_lengths[index] != 0U) {
                value_bytes = server_runtime->pending_request.write_values[index];
                value_length = server_runtime->pending_request.write_value_lengths[index];
            }
            value = server_runtime_decode_write_unsigned_bytes(value_bytes, value_length);
            if (server_runtime_write_reference_matches_report_control_field(object_reference, "ResvTms")) {
                server_runtime->brcb_resv_tms = value;
                if (value != 0U && server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                    UnitLabMmsDiagnostic reserve_diagnostic;
                    unitlab_mms_diagnostic_clear(&reserve_diagnostic);
                    (void)unitlab_iec61850_report_control_reserve(&server_runtime->report_control, &reserve_diagnostic);
                } else if (value == 0U && server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
                    UnitLabMmsDiagnostic release_diagnostic;
                    unitlab_mms_diagnostic_clear(&release_diagnostic);
                    (void)unitlab_iec61850_report_control_release(&server_runtime->report_control, &release_diagnostic);
                }
                continue;
            }
            if (server_runtime_write_reference_matches_report_control_field(object_reference, "RptEna")) {
                server_runtime->brcb_rpt_ena = value != 0U ? 1U : 0U;
                if (server_runtime->brcb_rpt_ena != 0U) {
                    UnitLabMmsDiagnostic state_diagnostic;
                    unitlab_mms_diagnostic_clear(&state_diagnostic);
                    if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                        (void)unitlab_iec61850_report_control_reserve(&server_runtime->report_control, &state_diagnostic);
                    }
                    if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
                        (void)unitlab_iec61850_report_control_enable(&server_runtime->report_control, &state_diagnostic);
                    }
                } else if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_ENABLED
                           || server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_GI_PENDING
                           || server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_REPORTING) {
                    UnitLabMmsDiagnostic disable_diagnostic;
                    unitlab_mms_diagnostic_clear(&disable_diagnostic);
                    server_runtime->pending_gi_report = 0U;
                    server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_NONE;
                    server_runtime->pending_report_value_length = 0U;
                    (void)unitlab_iec61850_report_control_disable(&server_runtime->report_control, &disable_diagnostic);
                }
                continue;
            }
            if (server_runtime_write_reference_matches_report_control_field(object_reference, "GI")) {
                if (value != 0U && server_runtime->brcb_rpt_ena != 0U) {
                    UnitLabMmsDiagnostic gi_diagnostic;
                    unitlab_mms_diagnostic_clear(&gi_diagnostic);
                    if (unitlab_iec61850_report_control_request_gi(&server_runtime->report_control, &gi_diagnostic)) {
                        server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_GI;
                        server_runtime->pending_gi_report = 1U;
                    }
                }
                continue;
            }

            if (server_runtime_write_reference_matches_report_control_field(object_reference, "PurgeBuf")) {
                if (value != 0U) {
                    server_runtime->brcb_sq_num = 0U;
                    server_runtime->brcb_entry_id_counter = 0U;
                    memset(server_runtime->brcb_entry_id, 0, sizeof(server_runtime->brcb_entry_id));
                    memset(server_runtime->brcb_time_of_entry, 0, sizeof(server_runtime->brcb_time_of_entry));
                    server_runtime->pending_gi_report = 0U;
                    server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_NONE;
                    server_runtime->pending_report_value_length = 0U;
                }
                continue;
            }

            (void)unitlab_mms_server_runtime_queue_data_change_report_value(server_runtime, object_reference, value_bytes, value_length, NULL);
        }
    }

    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

int server_runtime_build_write_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t member_bytes[64U];
    uint8_t response_payload_bytes[96U];
    uint8_t service_bytes[128U];
    uint8_t invoke_id_element_bytes[16U];
    size_t member_length = 0U;
    size_t response_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;
    UnitLabMmsBerElement member_element;
    UnitLabMmsBerElement response_payload_element;
    size_t write_result_count = 1U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Write response service buffer and encoded_length are required.");
        return 0;
    }
    if (!server_runtime_apply_report_control_write(server_runtime, diagnostic)) {
        return 0;
    }

    if (server_runtime->pending_request.write_object_reference_count != 0U) {
        write_result_count = server_runtime->pending_request.write_object_reference_count;
    }
    for (size_t index = 0U; index < write_result_count; index++) {
        size_t single_member_length = 0U;

        unitlab_mms_ber_element_init(&member_element);
        member_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        member_element.tag.constructed = 0;
        member_element.tag.tag_number = 1U;
        member_element.value_bytes = NULL;
        member_element.value_length = 0U;
        if (!server_runtime_encode_ber_element(
                member_element.tag.tag_class,
                member_element.tag.constructed,
                member_element.tag.tag_number,
                member_element.value_bytes,
                member_element.value_length,
                member_bytes + member_length,
                sizeof(member_bytes) - member_length,
                &single_member_length,
                diagnostic)) {
            return 0;
        }
        member_length += single_member_length;
    }

    unitlab_mms_ber_element_init(&response_payload_element);
    response_payload_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    response_payload_element.tag.constructed = 1;
    response_payload_element.tag.tag_number = 5U;
    response_payload_element.value_bytes = member_bytes;
    response_payload_element.value_length = member_length;
    if (!server_runtime_encode_ber_element(
            response_payload_element.tag.tag_class,
            response_payload_element.tag.constructed,
            response_payload_element.tag.tag_number,
            response_payload_element.value_bytes,
            response_payload_element.value_length,
            response_payload_bytes,
            sizeof(response_payload_bytes),
            &response_payload_length,
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

    if (invoke_id_length + response_payload_length > sizeof(service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write response service buffer is too small.");
        return 0;
    }
    memcpy(service_bytes, invoke_id_element_bytes, invoke_id_length);
    memcpy(service_bytes + invoke_id_length, response_payload_bytes, response_payload_length);
    total_length = invoke_id_length + response_payload_length;
    if (total_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Write response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

