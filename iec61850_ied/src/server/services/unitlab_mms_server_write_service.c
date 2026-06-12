#include "server/unitlab_mms_server_runtime_internal.h"

#include <string.h>
#include <stdio.h>

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

static int server_runtime_decode_write_boolean(const uint8_t* value_bytes, size_t value_length, uint8_t* value)
{
    if (value_bytes == NULL || value == NULL || value_length != 1U || value_bytes[0] > 1U) {
        return 0;
    }
    *value = value_bytes[0];
    return 1;
}

static int server_runtime_decode_optional_fields_mask(const uint8_t* value_bytes, size_t value_length, uint8_t* mask)
{
    uint8_t wire_mask;
    uint8_t decoded_mask = 0U;

    if (value_bytes == NULL || mask == NULL || value_length != 3U || value_bytes[0] != 0x06U) {
        return 0;
    }
    if ((value_bytes[1] & 0x80U) != 0U || (value_bytes[2] & 0x7FU) != 0U) {
        return 0;
    }

    wire_mask = value_bytes[1];
    if ((wire_mask & 0x40U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM;
    }
    if ((wire_mask & 0x20U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP;
    }
    if ((wire_mask & 0x10U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION;
    }
    if ((wire_mask & 0x08U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_DATA_SET;
    }
    if ((wire_mask & 0x04U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE;
    }
    if ((wire_mask & 0x02U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW;
    }
    if ((wire_mask & 0x01U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID;
    }
    if ((value_bytes[2] & 0x80U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_RPT_OPT_CONF_REV;
    }

    *mask = decoded_mask;
    return 1;
}

static int server_runtime_decode_trigger_options_mask(const uint8_t* value_bytes, size_t value_length, uint8_t* mask)
{
    uint8_t wire_mask;
    uint8_t decoded_mask = 0U;

    if (value_bytes == NULL || mask == NULL || value_length != 2U || value_bytes[0] != 0x02U) {
        return 0;
    }
    wire_mask = value_bytes[1];
    if ((wire_mask & 0x03U) != 0U) {
        return 0;
    }
    if ((wire_mask & 0x40U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED;
    }
    if ((wire_mask & 0x20U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED;
    }
    if ((wire_mask & 0x10U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE;
    }
    if ((wire_mask & 0x08U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY;
    }
    if ((wire_mask & 0x04U) != 0U) {
        decoded_mask |= UNITLAB_IED_MODEL_TRG_OPT_GI;
    }
    *mask = decoded_mask;
    return 1;
}


#define UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED 0x09U

static int server_runtime_write_reference_matches_report_control_field(const char* object_reference, const char* field_name);

static void server_runtime_reset_write_results(UnitLabMmsServerRuntime* server_runtime, size_t write_count)
{
    if (server_runtime == NULL) {
        return;
    }
    memset(server_runtime->write_result_failures, 0, sizeof(server_runtime->write_result_failures));
    memset(server_runtime->write_result_error_codes, 0, sizeof(server_runtime->write_result_error_codes));
    server_runtime->write_result_count = write_count;
}

static void server_runtime_mark_write_failure(UnitLabMmsServerRuntime* server_runtime, size_t index, uint8_t error_code)
{
    if (server_runtime == NULL || index >= UNITLAB_MMS_MAX_READ_VARIABLES) {
        return;
    }
    server_runtime->write_result_failures[index] = 1U;
    server_runtime->write_result_error_codes[index] = error_code;
}

static const char* server_runtime_write_report_control_field_name(const char* object_reference)
{
    size_t field_count = 0U;
    const char* const* fields = server_runtime_report_control_block_fields(&field_count);

    if (object_reference == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < field_count; index++) {
        if (server_runtime_write_reference_matches_report_control_field(object_reference, fields[index])) {
            return fields[index];
        }
    }
    if (server_runtime_write_reference_matches_report_control_field(object_reference, "Owner")) {
        return "Owner";
    }
    if (server_runtime_write_reference_matches_report_control_field(object_reference, "Resv")) {
        return "Resv";
    }
    return NULL;
}

static int server_runtime_write_field_is_static_same_value_allowed(const char* field_name)
{
    return field_name != NULL
        && (strcmp(field_name, "DatSet") == 0
            || strcmp(field_name, "ConfRev") == 0
            || strcmp(field_name, "OptFlds") == 0
            || strcmp(field_name, "BufTm") == 0
            || strcmp(field_name, "TrgOps") == 0
            || strcmp(field_name, "IntgPd") == 0);
}

static int server_runtime_write_value_matches_current_report_field(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* field_name,
    const uint8_t* value_bytes,
    size_t value_length)
{
    char report_id_reference[160U];
    char data_set_reference[160U];
    uint8_t encoded_field[256U];
    size_t encoded_field_length = 0U;
    size_t consumed_length = 0U;
    UnitLabMmsBerElement field_element;
    UnitLabMmsDiagnostic diagnostic;

    if (server_runtime == NULL || field_name == NULL || value_bytes == NULL) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    report_id_reference[0] = '\0';
    data_set_reference[0] = '\0';
    server_runtime_format_report_control_references(server_runtime, report_id_reference, sizeof(report_id_reference), data_set_reference, sizeof(data_set_reference));
    if (!server_runtime_encode_report_control_block_field_value(
            server_runtime,
            field_name,
            report_id_reference,
            data_set_reference,
            encoded_field,
            sizeof(encoded_field),
            &encoded_field_length,
            &diagnostic)) {
        return 0;
    }
    unitlab_mms_ber_element_init(&field_element);
    if (!unitlab_mms_ber_read(&field_element, encoded_field, encoded_field_length, &consumed_length, &diagnostic) || consumed_length != encoded_field_length) {
        return 0;
    }
    return field_element.value_length == value_length && memcmp(field_element.value_bytes, value_bytes, value_length) == 0;
}


static int server_runtime_write_reference_matches_report_control_field(const char* object_reference, const char* field_name)
{
    if (object_reference == NULL || field_name == NULL) {
        return 0;
    }
    return server_runtime_object_reference_matches_report_control_field(object_reference, field_name);
}



int unitlab_mms_server_runtime_queue_data_change_report_value(
    UnitLabMmsServerRuntime* server_runtime,
    const char* object_reference,
    const uint8_t* value_bytes,
    size_t value_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (server_runtime == NULL || object_reference == NULL || value_bytes == NULL || value_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Data-change report queue requires runtime, object reference, and value bytes.");
        return 0;
    }
    if (server_runtime->brcb_rpt_ena == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Data-change report queue requires enabled BRCB.");
        return 0;
    }
    if (!unitlab_mms_server_runtime_update_signal_value(server_runtime, object_reference, value_bytes, value_length, diagnostic)) {
        return 0;
    }
    if (server_runtime->pending_report_kind != UNITLAB_MMS_SERVER_PENDING_REPORT_DATA_CHANGE || server_runtime->pending_report_value_length == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "Data-change report queue object is not a member of the active report DataSet.");
        return 0;
    }
    return 1;
}

static int server_runtime_apply_report_control_write(UnitLabMmsServerRuntime* server_runtime, UnitLabMmsDiagnostic* diagnostic)
{
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
        server_runtime_reset_write_results(server_runtime, write_count);

        for (size_t index = 0U; index < write_count; index++) {
            const char* object_reference = server_runtime->pending_request.object_reference;
            const uint8_t* value_bytes = server_runtime->pending_request.write_value;
            size_t value_length = server_runtime->pending_request.write_value_length;
            const char* rcb_field_name = NULL;
            uint32_t value = 0U;

            if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_object_references[index][0] != '\0') {
                object_reference = server_runtime->pending_request.write_object_references[index];
            }
            if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_value_lengths[index] != 0U) {
                value_bytes = server_runtime->pending_request.write_values[index];
                value_length = server_runtime->pending_request.write_value_lengths[index];
            }
            value = server_runtime_decode_write_unsigned_bytes(value_bytes, value_length);
            rcb_field_name = server_runtime_write_report_control_field_name(object_reference);

            if (rcb_field_name != NULL) {
                (void)server_runtime_select_report_control_by_reference(server_runtime, object_reference);
                if (strcmp(rcb_field_name, "ResvTms") == 0) {
                    if (server_runtime->brcb_rpt_ena != 0U) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    server_runtime->brcb_resv_tms = value;
                    if (value != 0U && server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                        UnitLabMmsDiagnostic reserve_diagnostic;
                        unitlab_mms_diagnostic_clear(&reserve_diagnostic);
                        snprintf(server_runtime->brcb_owner, sizeof(server_runtime->brcb_owner), "%s", "local-client");
                        (void)unitlab_iec61850_report_control_reserve(&server_runtime->report_control, &reserve_diagnostic);
                    } else if (value == 0U && server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
                        UnitLabMmsDiagnostic release_diagnostic;
                        unitlab_mms_diagnostic_clear(&release_diagnostic);
                        (void)unitlab_iec61850_report_control_release(&server_runtime->report_control, &release_diagnostic);
                        server_runtime->brcb_owner[0] = 0;
                    }
                    continue;
                }
                if (strcmp(rcb_field_name, "Resv") == 0) {
                    uint8_t boolean_value = 0U;
                    if (!server_runtime_decode_write_boolean(value_bytes, value_length, &boolean_value)) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    value = boolean_value;
                    if (value != 0U) {
                        if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                            UnitLabMmsDiagnostic reserve_diagnostic;
                            unitlab_mms_diagnostic_clear(&reserve_diagnostic);
                            (void)unitlab_iec61850_report_control_reserve(&server_runtime->report_control, &reserve_diagnostic);
                        } else if (server_runtime->report_control.state != UNITLAB_IEC61850_REPORT_CONTROL_RESERVED) {
                            server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        }
                    } else {
                        if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_RESERVED || server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                            UnitLabMmsDiagnostic release_diagnostic;
                            unitlab_mms_diagnostic_clear(&release_diagnostic);
                            (void)unitlab_iec61850_report_control_release(&server_runtime->report_control, &release_diagnostic);
                        } else {
                            server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        }
                    }
                    continue;
                }
                if (strcmp(rcb_field_name, "RptEna") == 0) {
                    uint8_t boolean_value = 0U;
                    if (!server_runtime_decode_write_boolean(value_bytes, value_length, &boolean_value)) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    server_runtime->brcb_rpt_ena = boolean_value;
                    if (server_runtime->brcb_rpt_ena != 0U) {
                        UnitLabMmsDiagnostic state_diagnostic;
                        unitlab_mms_diagnostic_clear(&state_diagnostic);
                        if (server_runtime->report_control.state == UNITLAB_IEC61850_REPORT_CONTROL_DISABLED) {
                            snprintf(server_runtime->brcb_owner, sizeof(server_runtime->brcb_owner), "%s", "local-client");
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
                        server_runtime_clear_pending_reports(server_runtime);
                        (void)unitlab_iec61850_report_control_disable(&server_runtime->report_control, &disable_diagnostic);
                        if (server_runtime->brcb_resv_tms == 0U) {
                            server_runtime->brcb_owner[0] = 0;
                        }
                    }
                    continue;
                }
                if (strcmp(rcb_field_name, "GI") == 0) {
                    uint8_t boolean_value = 0U;
                    if (!server_runtime_decode_write_boolean(value_bytes, value_length, &boolean_value)) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    value = boolean_value;
                    if (value != 0U && server_runtime->brcb_rpt_ena != 0U) {
                        UnitLabMmsDiagnostic gi_diagnostic;
                        unitlab_mms_diagnostic_clear(&gi_diagnostic);
                        if (unitlab_iec61850_report_control_request_gi(&server_runtime->report_control, &gi_diagnostic)) {
                            server_runtime->pending_report_kind = UNITLAB_MMS_SERVER_PENDING_REPORT_GI;
                            server_runtime->pending_gi_report = 1U;
                        } else {
                            server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        }
                    } else if (value != 0U) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                    }
                    continue;
                }
                if (strcmp(rcb_field_name, "PurgeBuf") == 0) {
                    uint8_t boolean_value = 0U;
                    if (!server_runtime_decode_write_boolean(value_bytes, value_length, &boolean_value)) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    value = boolean_value;
                    if (value != 0U && server_runtime->brcb_rpt_ena != 0U) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    if (value != 0U) {
                        server_runtime->brcb_sq_num = 0U;
                        server_runtime->brcb_entry_id_counter = 0U;
                        memset(server_runtime->brcb_entry_id, 0, sizeof(server_runtime->brcb_entry_id));
                        memset(server_runtime->brcb_time_of_entry, 0, sizeof(server_runtime->brcb_time_of_entry));
                        server_runtime_clear_pending_reports(server_runtime);
                    }
                    continue;
                }
                if (strcmp(rcb_field_name, "OptFlds") == 0) {
                    uint8_t mask = 0U;
                    if (server_runtime->brcb_rpt_ena != 0U || !server_runtime_decode_optional_fields_mask(value_bytes, value_length, &mask)) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    server_runtime->brcb_optional_fields_mask_known = 1U;
                    server_runtime->brcb_optional_fields_mask = mask;
                    printf(
                        "native-wire-server: rcb-write field=OptFlds mask=%u sequence-number=%s report-time-stamp=%s reason-for-inclusion=%s data-set-name=%s data-reference=%s buf-ovfl=%s entry-id=%s conf-rev=%s\n",
                        (unsigned)mask,
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_DATA_SET) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_RPT_OPT_CONF_REV) != 0U ? "true" : "false");
                    fflush(stdout);
                    continue;
                }
                if (strcmp(rcb_field_name, "TrgOps") == 0) {
                    uint8_t mask = 0U;
                    if (server_runtime->brcb_rpt_ena != 0U || !server_runtime_decode_trigger_options_mask(value_bytes, value_length, &mask)) {
                        server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                        continue;
                    }
                    server_runtime->brcb_trigger_options_mask_known = 1U;
                    server_runtime->brcb_trigger_options_mask = mask;
                    server_runtime_clear_pending_reports(server_runtime);
                    printf(
                        "native-wire-server: rcb-write field=TrgOps mask=%u data-change=%s quality-change=%s data-update=%s integrity=%s gi=%s\n",
                        (unsigned)mask,
                        (mask & UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY) != 0U ? "true" : "false",
                        (mask & UNITLAB_IED_MODEL_TRG_OPT_GI) != 0U ? "true" : "false");
                    fflush(stdout);
                    continue;
                }
                if (server_runtime_write_field_is_static_same_value_allowed(rcb_field_name)
                    && server_runtime->brcb_rpt_ena == 0U
                    && server_runtime_write_value_matches_current_report_field(server_runtime, rcb_field_name, value_bytes, value_length)) {
                    continue;
                }
                server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
                continue;
            }

            if (!unitlab_mms_server_runtime_update_signal_value(server_runtime, object_reference, value_bytes, value_length, NULL)) {
                server_runtime_mark_write_failure(server_runtime, index, UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED);
            }
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
        const char* object_reference = server_runtime->pending_request.object_reference;
        const char* field_name = NULL;

        uint8_t failure_code[1U] = { UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED };

        if (index < server_runtime->pending_request.write_object_reference_count && server_runtime->pending_request.write_object_references[index][0] != '\0') {
            object_reference = server_runtime->pending_request.write_object_references[index];
        }
        field_name = server_runtime_write_report_control_field_name(object_reference);

        unitlab_mms_ber_element_init(&member_element);
        member_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
        member_element.tag.constructed = 0;
        if (index < server_runtime->write_result_count && server_runtime->write_result_failures[index] != 0U) {
            failure_code[0] = server_runtime->write_result_error_codes[index] != 0U ? server_runtime->write_result_error_codes[index] : UNITLAB_MMS_WRITE_DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
            member_element.tag.tag_number = 0U;
            member_element.value_bytes = failure_code;
            member_element.value_length = sizeof(failure_code);
        } else {
            member_element.tag.tag_number = 1U;
            member_element.value_bytes = NULL;
            member_element.value_length = 0U;
        }
        printf(
            "native-wire-server: write-result invoke=%u index=%zu object=%s field=%s status=%s error=%u\n",
            (unsigned)invoke_id,
            index,
            object_reference != NULL && object_reference[0] != '\0' ? object_reference : "<none>",
            field_name != NULL ? field_name : "<none>",
            member_element.tag.tag_number == 0U ? "failure" : "success",
            member_element.tag.tag_number == 0U ? (unsigned)failure_code[0] : 0U);
        fflush(stdout);
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

