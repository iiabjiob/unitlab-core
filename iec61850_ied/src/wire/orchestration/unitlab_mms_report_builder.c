#include "unitlab_mms_wire_builder_internal.h"

#include <string.h>

int unitlab_mms_build_information_report_frame(
    const char* item_id,
    uint8_t boolean_value,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    const char* effective_item_id = (item_id != NULL && item_id[0] != '\0') ? item_id : "RPT";
    uint8_t item_id_bytes[64U];
    uint8_t variable_spec_bytes[80U];
    uint8_t sequence_bytes[96U];
    uint8_t list_of_variables_bytes[112U];
    uint8_t access_result_bytes[8U];
    uint8_t list_of_access_results_bytes[16U];
    uint8_t report_content_bytes[160U];
    uint8_t report_pdu_bytes[192U];
    size_t item_id_length = 0U;
    size_t variable_spec_length = 0U;
    size_t sequence_length = 0U;
    size_t list_of_variables_length = 0U;
    size_t access_result_length = 0U;
    size_t list_of_access_results_length = 0U;
    size_t report_content_length = 0U;
    size_t report_pdu_length = 0U;
    UnitLabMmsPdu report_pdu;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (scratch == NULL || buffer == NULL || encoded_length == NULL) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Information report frame requires scratch, buffer, and encoded_length.");
        return 0;
    }
    if (scratch_length == 0U || buffer_length == 0U) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Information report frame scratch and buffer must be non-zero.");
        return 0;
    }

    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            (const uint8_t*)effective_item_id,
            strlen(effective_item_id),
            item_id_bytes,
            sizeof(item_id_bytes),
            &item_id_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            item_id_bytes,
            item_id_length,
            variable_spec_bytes,
            sizeof(variable_spec_bytes),
            &variable_spec_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            variable_spec_bytes,
            variable_spec_length,
            sequence_bytes,
            sizeof(sequence_bytes),
            &sequence_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            sequence_bytes,
            sequence_length,
            list_of_variables_bytes,
            sizeof(list_of_variables_bytes),
            &list_of_variables_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            3U,
            &boolean_value,
            1U,
            access_result_bytes,
            sizeof(access_result_bytes),
            &access_result_length,
            diagnostic)) {
        return 0;
    }
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            access_result_bytes,
            access_result_length,
            list_of_access_results_bytes,
            sizeof(list_of_access_results_bytes),
            &list_of_access_results_length,
            diagnostic)) {
        return 0;
    }
    if (list_of_variables_length + list_of_access_results_length > sizeof(report_content_bytes)) {
        wire_builder_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Information report content buffer is too small.");
        return 0;
    }
    memcpy(report_content_bytes, list_of_variables_bytes, list_of_variables_length);
    memcpy(report_content_bytes + list_of_variables_length, list_of_access_results_bytes, list_of_access_results_length);
    report_content_length = list_of_variables_length + list_of_access_results_length;
    if (!wire_builder_encode_nested_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            3U,
            report_content_bytes,
            report_content_length,
            report_pdu_bytes,
            sizeof(report_pdu_bytes),
            &report_pdu_length,
            diagnostic)) {
        return 0;
    }

    unitlab_mms_pdu_init(&report_pdu);
    report_pdu.kind = UNITLAB_MMS_PDU_UNCONFIRMED;
    report_pdu.has_service = 1;
    report_pdu.service_kind = UNITLAB_MMS_SERVICE_INFORMATION_REPORT;
    report_pdu.pdu_bytes = report_pdu_bytes;
    report_pdu.pdu_length = report_pdu_length;
    return unitlab_mms_build_wire_frame_from_pdu(&report_pdu, scratch, scratch_length, buffer, buffer_length, encoded_length, diagnostic);
}

