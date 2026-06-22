#define _POSIX_C_SOURCE 200112L
#include "native_wire_client.h"
#include "native_wire_client_ber_helpers.h"
#include "native_wire_client_session.h"
#include "native_wire_client_discovery.h"
#include "native_wire_session_backend.h"
#include "native_wire_session_runtime.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#include "wire/orchestration/unitlab_mms_association_frame.h"
#include "wire/acse/unitlab_mms_acse.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_live_wire_probe.h"
#include "wire/orchestration/unitlab_mms_wire_builder.h"

typedef enum {
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_INIT,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_DATA_CONNECTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_CONTROL_CONNECTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_COTP_CONNECTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATING,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_ATTRIBUTES_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_REPORT_REQUESTED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED,
    UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED,
} UnitLabNativeWireClientState;

static const char* state_name(UnitLabNativeWireClientState state)
{
    switch (state) {
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_INIT:
        return "init";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_DATA_CONNECTED:
        return "data-connected";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_CONTROL_CONNECTED:
        return "control-connected";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_COTP_CONNECTED:
        return "cotp-connected";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATING:
        return "associating";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATED:
        return "associated";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY:
        return "ready";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED:
        return "read-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED:
        return "get-name-list-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_ATTRIBUTES_REQUESTED:
        return "attributes-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED:
        return "write-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_REPORT_REQUESTED:
        return "report-requested";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED:
        return "stopped";
    case UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED:
        return "failed";
    }
    return "unknown";
}

static int emit_text_response(const char* text);

static int emit_state_response(UnitLabNativeWireClientState state)
{
    char text[96U];
    snprintf(text, sizeof(text), "native-wire-client: state=%s", state_name(state));
    return emit_text_response(text);
}

static void set_result(UnitLabIedModelLoadResult* result, const char* code, const char* message)
{
    if (result == NULL) {
        return;
    }
    result->loaded = 0;
    snprintf(result->code, sizeof(result->code), "%s", code);
    snprintf(result->message, sizeof(result->message), "%s", message);
}

static void emit_subscription_summary(const UnitLabNativeClientSessionState* session, const char* phase)
{
    if (session == NULL) {
        return;
    }
    printf(
        "native-wire-client: subscription-summary phase=%s rcb=%s/%s rcb-index=%zu rptEna=%s rptEna-invoke=%u giRequested=%s gi-invoke=%u lastReportReceived=%s asyncReports=%zu lastReportSequenceKnown=%s lastReportSequence=%u lastReportSubSequenceKnown=%s lastReportSubSequence=%u lastReportSequenceGeneration=%llu seqGapCount=%llu seqDuplicateCount=%llu seqOutOfOrderCount=%llu seqDropCount=%llu seqMissingCount=%llu seqWrapCount=%llu reportHealth=%s reportHealthReason=%s lastReportValues=%zu lastReportDataRefs=%zu lastReportMatchedDataRefs=%zu lastReportReasons=%zu lastReportDatasetMismatches=%zu lastReportMissingValues=%zu lastReportExtraValues=%zu lastReportMissingReasons=%zu lastReportExtraReasons=%zu lastReportUnsupportedValues=%zu\n",
        phase != NULL ? phase : "snapshot",
        session->subscription_model.rcb_domain[0] != '\0' ? session->subscription_model.rcb_domain : "<none>",
        session->subscription_model.rcb_item[0] != '\0' ? session->subscription_model.rcb_item : "<none>",
        session->subscription_model.selected_rcb_index,
        session->subscription_model.rpt_enabled ? "true" : "false",
        session->subscription_model.last_rptena_invoke_id,
        session->subscription_model.gi_requested ? "true" : "false",
        session->subscription_model.last_gi_invoke_id,
        session->subscription_model.last_report_received ? "true" : "false",
        session->subscription_model.async_report_count,
        session->subscription_model.has_last_report_sequence_number ? "true" : "false",
        session->subscription_model.has_last_report_sequence_number ? session->subscription_model.last_report_sequence_number : 0U,
        session->subscription_model.has_last_report_sub_sequence_number ? "true" : "false",
        session->subscription_model.has_last_report_sub_sequence_number ? session->subscription_model.last_report_sub_sequence_number : 0U,
        (unsigned long long)session->subscription_model.last_report_sequence_generation,
        (unsigned long long)session->subscription_model.report_sequence_gap_count,
        (unsigned long long)session->subscription_model.report_sequence_duplicate_count,
        (unsigned long long)session->subscription_model.report_sequence_out_of_order_count,
        (unsigned long long)session->subscription_model.report_sequence_drop_count,
        (unsigned long long)session->subscription_model.report_sequence_missing_count,
        (unsigned long long)session->subscription_model.report_sequence_wrap_count,
        session->subscription_model.report_health[0] != '\0' ? session->subscription_model.report_health : "<none>",
        session->subscription_model.report_health_reason[0] != '\0' ? session->subscription_model.report_health_reason : "<none>",
        session->discovered_model.last_report_value_count,
        session->discovered_model.last_report_data_ref_count,
        session->discovered_model.last_report_matched_data_ref_count,
        session->discovered_model.last_report_reason_count,
        session->discovered_model.last_report_dataset_mismatch_count,
        session->discovered_model.last_report_missing_value_count,
        session->discovered_model.last_report_extra_value_count,
        session->discovered_model.last_report_missing_reason_count,
        session->discovered_model.last_report_extra_reason_count,
        session->discovered_model.last_report_unsupported_value_count);
    fflush(stdout);
}

static void emit_discovered_model_summary(const UnitLabNativeClientSessionState* session, const char* phase)
{
    size_t typed_data_name_count = 0U;
    size_t typed_data_component_count = 0U;

    if (session == NULL) {
        return;
    }
    for (size_t index = 0U; index < session->discovered_data_name_count; index++) {
        if (session->discovered_data_names[index].type_kind[0] != '\0' && strcmp(session->discovered_data_names[index].type_kind, "unknown") != 0) {
            typed_data_name_count++;
        }
    }
    for (size_t index = 0U; index < session->discovered_data_component_count; index++) {
        if (session->discovered_data_components[index].type_kind[0] != '\0' && strcmp(session->discovered_data_components[index].type_kind, "unknown") != 0) {
            typed_data_component_count++;
        }
    }
    printf(
        "native-wire-client: model-summary phase=%s domain=%s logical-devices=%zu logical-nodes=%zu data-names=%zu typed-data-names=%zu data-components=%zu typed-data-components=%zu typed-data-nodes=%zu typed-leaf-nodes=%zu leaf-refs=%zu datasets=%zu dataset-members=%zu brcbs=%zu gva-success-count=%zu gva-failed-count=%zu unsupported-type-count=%zu skipped-branch-count=%zu max-depth-seen=%zu array-count=%zu quality-like-count=%zu timestamp-like-count=%zu last-report-entries=%zu last-report-dataRefs=%zu last-report-values=%zu last-report-reasons=%zu last-report-matched-dataRefs=%zu last-report-dataset-mismatches=%zu last-report-missing-values=%zu last-report-extra-values=%zu last-report-missing-reasons=%zu last-report-extra-reasons=%zu last-report-unsupported-values=%zu last-report-rptId=%s last-report-datSet=%s\n",
        phase != NULL ? phase : "snapshot",
        session->discovered_model.domain[0] != '\0' ? session->discovered_model.domain : "<none>",
        session->discovered_model.logical_device_count,
        session->discovered_model.logical_node_count,
        session->discovered_model.data_name_count,
        typed_data_name_count,
        session->discovered_model.data_component_count,
        typed_data_component_count,
        session->discovered_model.typed_data_node_count,
        session->discovered_model.typed_leaf_count,
        session->discovered_model.leaf_ref_count,
        session->discovered_model.data_set_count,
        session->discovered_model.data_set_member_count,
        session->discovered_model.brcb_count,
        session->discovered_model.gva_success_count,
        session->discovered_model.gva_failed_count,
        session->discovered_model.unsupported_type_count,
        session->discovered_model.skipped_branch_count,
        session->discovered_model.max_depth_seen,
        session->discovered_model.array_count,
        session->discovered_model.quality_like_count,
        session->discovered_model.timestamp_like_count,
        session->last_report_entry_count,
        session->discovered_model.last_report_data_ref_count,
        session->discovered_model.last_report_value_count,
        session->discovered_model.last_report_reason_count,
        session->discovered_model.last_report_matched_data_ref_count,
        session->discovered_model.last_report_dataset_mismatch_count,
        session->discovered_model.last_report_missing_value_count,
        session->discovered_model.last_report_extra_value_count,
        session->discovered_model.last_report_missing_reason_count,
        session->discovered_model.last_report_extra_reason_count,
        session->discovered_model.last_report_unsupported_value_count,
        session->discovered_model.last_report_rpt_id[0] != '\0' ? session->discovered_model.last_report_rpt_id : "<none>",
        session->discovered_model.last_report_data_set[0] != '\0' ? session->discovered_model.last_report_data_set : "<none>");
    fflush(stdout);
}

static int send_all(int fd, const uint8_t* buffer, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t written = send(fd, buffer + offset, length - offset, MSG_NOSIGNAL);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (written == 0) {
            return 0;
        }
        offset += (size_t)written;
    }
    return 1;
}

static int connect_socket(const char* host, int port)
{
    struct addrinfo hints;
    struct addrinfo* info = NULL;
    char port_text[16U];
    int fd = -1;

    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535) {
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_text, sizeof(port_text), "%d", port);
    if (getaddrinfo(host, port_text, &hints, &info) != 0) {
        return -1;
    }

    for (struct addrinfo* current = info; current != NULL; current = current->ai_next) {
        fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(info);
    return fd;
}

static uint64_t native_wire_now_ms(void)
{
    time_t now = time(NULL);
    if (now <= (time_t)0) {
        return 0U;
    }
    return (uint64_t)now * 1000U;
}

static void native_wire_format_endpoint_id(const UnitLabIedServerConfig* config, char* buffer, size_t buffer_length)
{
    if (buffer == NULL || buffer_length == 0U) {
        return;
    }
    buffer[0] = '\0';
    if (config == NULL || config->bind_address == NULL || config->bind_address[0] == '\0' || config->port <= 0) {
        return;
    }
    snprintf(buffer, buffer_length, "mms:%s:%d", config->bind_address, config->port);
}

static void native_wire_copy_text(char* destination, size_t destination_size, const char* source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, destination_size - 1U);
    destination[destination_size - 1U] = '\0';
}

static const char* pdu_kind_label(UnitLabMmsPduKind kind)
{
    switch (kind) {
    case UNITLAB_MMS_PDU_CONFIRMED_REQUEST:
        return "confirmed-request";
    case UNITLAB_MMS_PDU_CONFIRMED_RESPONSE:
        return "confirmed-response";
    case UNITLAB_MMS_PDU_CONFIRMED_ERROR:
        return "confirmed-error";
    case UNITLAB_MMS_PDU_UNCONFIRMED:
        return "unconfirmed";
    case UNITLAB_MMS_PDU_INITIATE_RESPONSE:
        return "initiate-response";
    default:
        return "other";
    }
}

static const char* service_kind_label(UnitLabMmsServiceKind kind)
{
    switch (kind) {
    case UNITLAB_MMS_SERVICE_READ:
        return "read";
    case UNITLAB_MMS_SERVICE_WRITE:
        return "write";
    case UNITLAB_MMS_SERVICE_GET_NAME_LIST:
        return "get-name-list";
    case UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES:
        return "get-variable-access-attributes";
    case UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES:
        return "get-named-variable-list-attributes";
    case UNITLAB_MMS_SERVICE_INFORMATION_REPORT:
        return "information-report";
    default:
        return "raw";
    }
}

static const char* access_result_label(const UnitLabMmsBerElement* element)
{
    if (element == NULL) {
        return "unknown";
    }
    if (element->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && element->tag.tag_number == 0U) {
        return "failure";
    }
    return "success";
}

static const char* report_sequence_disposition_label(UnitLabNativeReportSequenceDisposition disposition)
{
    switch (disposition) {
    case UNITLAB_NATIVE_REPORT_SEQUENCE_ACCEPTED:
        return "accepted";
    case UNITLAB_NATIVE_REPORT_SEQUENCE_DUPLICATE:
        return "duplicate";
    case UNITLAB_NATIVE_REPORT_SEQUENCE_OUT_OF_ORDER:
        return "out-of-order";
    case UNITLAB_NATIVE_REPORT_SEQUENCE_GAP:
        return "gap";
    }
    return "unknown";
}

static void print_hex_value(const uint8_t* bytes, size_t length)
{
    static const char hex_digits[] = "0123456789abcdef";
    size_t limit = length < 32U ? length : 32U;

    for (size_t index = 0U; index < limit; index++) {
        fputc(hex_digits[(bytes[index] >> 4U) & 0x0FU], stdout);
        fputc(hex_digits[bytes[index] & 0x0FU], stdout);
    }
    if (length > limit) {
        fputs("...", stdout);
    }
}

static uint32_t decode_unsigned_bytes(const uint8_t* bytes, size_t length)
{
    uint32_t value = 0U;
    for (size_t index = 0U; index < length && index < 4U; index++) {
        value = (uint32_t)((value << 8U) | bytes[index]);
    }
    return value;
}

static int validate_confirmed_write_response(
    const uint8_t* response,
    size_t response_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsBerElement result;
    size_t consumed = 0U;
    size_t offset = 0U;

    if (response == NULL || response_length == 0U) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client received an empty confirmed-write response.");
        }
        return 0;
    }

    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, response, response_length, &consumed, diagnostic)
        || association_frame.presentation.payload_bytes == NULL
        || association_frame.presentation.payload_length == 0U) {
        if (diagnostic != NULL && diagnostic->message[0] == '\0') {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not decode the confirmed-write response frame.");
        }
        return 0;
    }

    unitlab_mms_pdu_init(&pdu);
    if (!unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed, diagnostic)
        || pdu.kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        || pdu.service_kind != UNITLAB_MMS_SERVICE_WRITE
        || pdu.service_bytes == NULL
        || pdu.service_length == 0U) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client received an invalid confirmed-write response.");
        }
        return 0;
    }

    while (offset < pdu.service_length) {
        size_t result_consumed = 0U;
        unitlab_mms_ber_element_init(&result);
        if (!unitlab_mms_ber_read(&result, &pdu.service_bytes[offset], pdu.service_length - offset, &result_consumed, diagnostic)
            || result_consumed == 0U) {
            if (diagnostic != NULL) {
                diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not decode the confirmed-write access result.");
            }
            return 0;
        }
        if (result.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && result.tag.tag_number == 0U) {
            unsigned code = (unsigned)decode_unsigned_bytes(result.value_bytes, result.value_length);
            if (diagnostic != NULL) {
                diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(diagnostic->message, sizeof(diagnostic->message), "Native wire client confirmed-write failed with access-result code=%u.", code);
            }
            return 0;
        }
        offset += result_consumed;
    }

    if (diagnostic != NULL) {
        unitlab_mms_diagnostic_clear(diagnostic);
    }
    return 1;
}

static const char* brcb_field_name(size_t index)
{
    static const char* fields[] = {
        "RptID",
        "RptEna",
        "DatSet",
        "ConfRev",
        "OptFlds",
        "BufTm",
        "SqNum",
        "TrgOps",
        "IntgPd",
        "GI",
        "PurgeBuf",
        "EntryID",
        "TimeofEntry",
        "ResvTms",
    };
    return index < sizeof(fields) / sizeof(fields[0]) ? fields[index] : NULL;
}

static const char* urcb_field_name(size_t index)
{
    static const char* fields[] = {
        "RptID",
        "RptEna",
        "Resv",
        "DatSet",
        "ConfRev",
        "OptFlds",
        "BufTm",
        "SqNum",
        "TrgOps",
        "IntgPd",
        "GI",
    };
    return index < sizeof(fields) / sizeof(fields[0]) ? fields[index] : NULL;
}

static size_t count_constructed_children(const UnitLabMmsBerElement* element)
{
    UnitLabMmsDiagnostic diagnostic;
    size_t offset = 0U;
    size_t count = 0U;

    if (element == NULL || !element->tag.constructed || element->value_bytes == NULL) {
        return 0U;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    while (offset < element->value_length) {
        UnitLabMmsBerElement child;
        size_t consumed = 0U;
        unitlab_mms_ber_element_init(&child);
        if (!unitlab_mms_ber_read(&child, &element->value_bytes[offset], element->value_length - offset, &consumed, &diagnostic) || consumed == 0U) {
            return count;
        }
        count++;
        offset += consumed;
    }
    return count;
}

static void print_data_value_summary(const UnitLabMmsBerElement* value)
{
    if (value == NULL || value->value_bytes == NULL || value->value_length == 0U) {
        printf("<empty>");
        return;
    }
    if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 3U && value->value_length == 1U) {
        printf("%s", value->value_bytes[0] != 0U ? "true" : "false");
    } else if (unitlab_native_client_bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
        size_t printable_length = value->value_length < 96U ? value->value_length : 96U;
        printf("\"");
        fwrite(value->value_bytes, 1U, printable_length, stdout);
        if (value->value_length > printable_length) {
            fputs("...", stdout);
        }
        printf("\"");
    } else if (value->value_length <= 4U && (value->tag.tag_number == 5U || value->tag.tag_number == 6U)) {
        printf("%u", (unsigned)decode_unsigned_bytes(value->value_bytes, value->value_length));
    } else {
        printf("0x");
        print_hex_value(value->value_bytes, value->value_length);
    }
}

static void append_hex_summary(char* buffer, size_t buffer_size, const uint8_t* bytes, size_t length)
{
    static const char hex[] = "0123456789abcdef";
    size_t used;

    if (buffer == NULL || buffer_size == 0U || bytes == NULL) {
        return;
    }
    used = strlen(buffer);
    for (size_t index = 0U; index < length && used + 2U < buffer_size; index++) {
        buffer[used++] = hex[(bytes[index] >> 4U) & 0x0FU];
        buffer[used++] = hex[bytes[index] & 0x0FU];
    }
    buffer[used] = '\0';
}

static void copy_data_value_summary(const UnitLabMmsBerElement* value, char* buffer, size_t buffer_size)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement first_child;
    size_t consumed = 0U;

    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    buffer[0] = '\0';
    if (value == NULL || value->value_bytes == NULL || value->value_length == 0U) {
        snprintf(buffer, buffer_size, "%s", "<empty>");
        return;
    }
    if (value->tag.constructed) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_ber_element_init(&first_child);
        if (unitlab_mms_ber_read(&first_child, value->value_bytes, value->value_length, &consumed, &diagnostic) && consumed != 0U) {
            copy_data_value_summary(&first_child, buffer, buffer_size);
            return;
        }
    }
    if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 3U && value->value_length == 1U) {
        snprintf(buffer, buffer_size, "%s", value->value_bytes[0] != 0U ? "true" : "false");
    } else if (unitlab_native_client_bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
        size_t printable_length = value->value_length < 96U ? value->value_length : 96U;
        size_t copy_length = printable_length < buffer_size - 1U ? printable_length : buffer_size - 1U;
        if (buffer_size > 2U) {
            buffer[0] = '"';
            copy_length = printable_length < buffer_size - 3U ? printable_length : buffer_size - 3U;
            memcpy(&buffer[1], value->value_bytes, copy_length);
            buffer[1U + copy_length] = '"';
            buffer[2U + copy_length] = '\0';
        }
        if (value->value_length > printable_length && strlen(buffer) + 3U < buffer_size) {
            strncat(buffer, "...", buffer_size - strlen(buffer) - 1U);
        }
    } else if (value->value_length <= 4U && (value->tag.tag_number == 5U || value->tag.tag_number == 6U)) {
        snprintf(buffer, buffer_size, "%u", (unsigned)decode_unsigned_bytes(value->value_bytes, value->value_length));
    } else {
        snprintf(buffer, buffer_size, "%s", "0x");
        append_hex_summary(buffer, buffer_size, value->value_bytes, value->value_length);
    }
}

static const char* report_value_kind_label(UnitLabNativeReportValueKind kind)
{
    switch (kind) {
        case UNITLAB_NATIVE_REPORT_VALUE_EMPTY:
            return "empty";
        case UNITLAB_NATIVE_REPORT_VALUE_BOOL:
            return "bool";
        case UNITLAB_NATIVE_REPORT_VALUE_UNSIGNED:
            return "unsigned";
        case UNITLAB_NATIVE_REPORT_VALUE_INTEGER:
            return "integer";
        case UNITLAB_NATIVE_REPORT_VALUE_FLOAT:
            return "float";
        case UNITLAB_NATIVE_REPORT_VALUE_STRING:
            return "string";
        case UNITLAB_NATIVE_REPORT_VALUE_OCTETS:
            return "octets";
        case UNITLAB_NATIVE_REPORT_VALUE_BIT_STRING:
            return "bit-string";
        case UNITLAB_NATIVE_REPORT_VALUE_STRUCTURE:
            return "structure";
        case UNITLAB_NATIVE_REPORT_VALUE_UNSUPPORTED:
        default:
            return "unsupported";
    }
}

static void emit_report_entry_contract(const UnitLabNativeClientSessionState* session)
{
    if (session == NULL) {
        return;
    }
    for (size_t index = 0U; index < session->last_report_entry_count; index++) {
        const UnitLabNativeLastReportEntry* entry = &session->last_report_entries[index];
        printf(
            "native-wire-client: report-entry index=%zu reference=%s dataRef=%s value=%s kind=%s reason=%s datasetMatch=%s discoveredMatch=%s\n",
            entry->inclusion_index,
            entry->display_reference[0] != '\0' ? entry->display_reference : entry->data_reference,
            entry->data_reference[0] != '\0' ? entry->data_reference : "<none>",
            entry->value_summary[0] != '\0' ? entry->value_summary : "<none>",
            report_value_kind_label(entry->value_kind),
            entry->reason_labels[0] != '\0' ? entry->reason_labels : "unknown",
            entry->dataset_match ? "true" : "false",
            entry->discovered_match ? "true" : "false");
    }
    fflush(stdout);
}

static int64_t decode_signed_bytes(const uint8_t* bytes, size_t length)
{
    uint64_t unsigned_value = decode_unsigned_bytes(bytes, length);
    uint64_t sign_bit;

    if (bytes == NULL || length == 0U || length >= sizeof(uint64_t)) {
        return (int64_t)unsigned_value;
    }
    sign_bit = 1ULL << ((length * 8U) - 1U);
    if ((unsigned_value & sign_bit) == 0U) {
        return (int64_t)unsigned_value;
    }
    return (int64_t)(unsigned_value | (~0ULL << (length * 8U)));
}

static int decode_mms_float32_value(const uint8_t* bytes, size_t length, double* value)
{
    uint32_t real_bits;
    float real_value;

    if (bytes == NULL || value == NULL || length != 5U || bytes[0] != 0x08U) {
        return 0;
    }
    real_bits = ((uint32_t)bytes[1] << 24U) | ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 8U) | (uint32_t)bytes[4];
    memcpy(&real_value, &real_bits, sizeof(real_value));
    *value = (double)real_value;
    return 1;
}

static void append_reason_label(char* labels, size_t labels_size, const char* label)
{
    if (labels == NULL || labels_size == 0U || label == NULL || label[0] == '\0') {
        return;
    }
    if (labels[0] != '\0') {
        strncat(labels, ",", labels_size - strlen(labels) - 1U);
    }
    strncat(labels, label, labels_size - strlen(labels) - 1U);
}

static void copy_report_reason_metadata(const UnitLabMmsBerElement* reason, UnitLabNativeLastReportEntry* entry)
{
    uint8_t reason_bits;

    if (entry == NULL) {
        return;
    }
    entry->raw_reason_tag_class = 0U;
    entry->raw_reason_tag_number = 0U;
    entry->raw_reason_length = 0U;
    entry->reason_code = 0U;
    entry->reason_flags = 0U;
    entry->reason_labels[0] = '\0';
    if (reason == NULL) {
        return;
    }
    entry->raw_reason_tag_class = (uint8_t)reason->tag.tag_class;
    entry->raw_reason_tag_number = (uint8_t)reason->tag.tag_number;
    entry->raw_reason_length = reason->value_length;
    if (reason->value_bytes == NULL || reason->value_length == 0U) {
        return;
    }
    entry->reason_code = decode_unsigned_bytes(reason->value_bytes, reason->value_length);
    reason_bits = reason->value_bytes[reason->value_length - 1U];
    if ((reason_bits & 0x40U) != 0U) {
        entry->reason_flags |= UNITLAB_NATIVE_REPORT_REASON_DATA_CHANGE;
        append_reason_label(entry->reason_labels, sizeof(entry->reason_labels), "data-change");
    }
    if ((reason_bits & 0x20U) != 0U) {
        entry->reason_flags |= UNITLAB_NATIVE_REPORT_REASON_QUALITY_CHANGE;
        append_reason_label(entry->reason_labels, sizeof(entry->reason_labels), "quality-change");
    }
    if ((reason_bits & 0x10U) != 0U) {
        entry->reason_flags |= UNITLAB_NATIVE_REPORT_REASON_DATA_UPDATE;
        append_reason_label(entry->reason_labels, sizeof(entry->reason_labels), "data-update");
    }
    if ((reason_bits & 0x08U) != 0U) {
        entry->reason_flags |= UNITLAB_NATIVE_REPORT_REASON_INTEGRITY;
        append_reason_label(entry->reason_labels, sizeof(entry->reason_labels), "integrity");
    }
    if ((reason_bits & 0x04U) != 0U) {
        entry->reason_flags |= UNITLAB_NATIVE_REPORT_REASON_GENERAL_INTERROGATION;
        append_reason_label(entry->reason_labels, sizeof(entry->reason_labels), "general-interrogation");
    }
    if ((reason_bits & 0x02U) != 0U) {
        entry->reason_flags |= UNITLAB_NATIVE_REPORT_REASON_APPLICATION_TRIGGER;
        append_reason_label(entry->reason_labels, sizeof(entry->reason_labels), "application-trigger");
    }
}

static int report_entry_is_quality_leaf(const UnitLabNativeLastReportEntry* entry)
{
    size_t length;

    if (entry == NULL) {
        return 0;
    }
    length = strlen(entry->data_reference);
    return length >= 2U && strcmp(&entry->data_reference[length - 2U], "$q") == 0;
}

static const char* quality_validity_label(uint8_t validity_bits)
{
    switch (validity_bits & 0xC0U) {
        case 0x00U:
            return "good";
        case 0x40U:
            return "invalid";
        case 0x80U:
            return "reserved";
        case 0xC0U:
            return "questionable";
        default:
            return "unknown";
    }
}

static void copy_report_quality_metadata(const UnitLabMmsBerElement* value, UnitLabNativeLastReportEntry* entry)
{
    if (entry == NULL) {
        return;
    }
    entry->quality_code = 0U;
    entry->quality_validity[0] = '\0';
    if (value == NULL
        || value->value_bytes == NULL
        || value->value_length < 2U
        || value->tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || value->tag.constructed
        || value->tag.tag_number != 4U
        || !report_entry_is_quality_leaf(entry)) {
        return;
    }
    entry->quality_code = decode_unsigned_bytes(value->value_bytes, value->value_length);
    snprintf(entry->quality_validity, sizeof(entry->quality_validity), "%s", quality_validity_label(value->value_bytes[1]));
}

static void copy_report_typed_value(const UnitLabMmsBerElement* value, UnitLabNativeLastReportEntry* entry)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement first_child;
    size_t consumed = 0U;

    if (entry == NULL) {
        return;
    }
    entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_UNSUPPORTED;
    entry->raw_tag_class = 0U;
    entry->raw_tag_number = 0U;
    entry->raw_value_length = 0U;
    entry->unsigned_value = 0U;
    entry->integer_value = 0;
    entry->floating_value = 0.0;
    entry->bool_value = 0;
    entry->quality_code = 0U;
    entry->quality_validity[0] = '\0';
    if (value == NULL) {
        return;
    }
    if (value->tag.constructed && value->value_bytes != NULL && value->value_length != 0U) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        unitlab_mms_ber_element_init(&first_child);
        if (unitlab_mms_ber_read(&first_child, value->value_bytes, value->value_length, &consumed, &diagnostic) && consumed != 0U) {
            copy_report_typed_value(&first_child, entry);
            return;
        }
    }
    entry->raw_tag_class = (uint8_t)value->tag.tag_class;
    entry->raw_tag_number = (uint8_t)value->tag.tag_number;
    entry->raw_value_length = value->value_length;
    if (value->value_bytes == NULL || value->value_length == 0U) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_EMPTY;
    } else if (value->tag.constructed) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_STRUCTURE;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 3U && value->value_length == 1U) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_BOOL;
        entry->bool_value = value->value_bytes[0] != 0U ? 1 : 0;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 5U && value->value_length <= sizeof(uint64_t)) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_INTEGER;
        entry->integer_value = decode_signed_bytes(value->value_bytes, value->value_length);
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 6U && value->value_length <= sizeof(uint64_t)) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_UNSIGNED;
        entry->unsigned_value = decode_unsigned_bytes(value->value_bytes, value->value_length);
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 7U && decode_mms_float32_value(value->value_bytes, value->value_length, &entry->floating_value)) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_FLOAT;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 4U) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_BIT_STRING;
        copy_report_quality_metadata(value, entry);
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 9U) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_OCTETS;
    } else if (unitlab_native_client_bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_STRING;
    }
}

static void copy_read_typed_value(const UnitLabMmsBerElement* value, UnitLabNativeLastReadResult* result)
{
    if (result == NULL) {
        return;
    }
    result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_UNSUPPORTED;
    result->raw_tag_class = 0U;
    result->raw_tag_number = 0U;
    result->raw_value_length = 0U;
    result->unsigned_value = 0U;
    result->integer_value = 0;
    result->floating_value = 0.0;
    result->bool_value = 0;
    if (value == NULL) {
        return;
    }
    result->raw_tag_class = (uint8_t)value->tag.tag_class;
    result->raw_tag_number = (uint8_t)value->tag.tag_number;
    result->raw_value_length = value->value_length;
    if (value->value_bytes == NULL || value->value_length == 0U) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_EMPTY;
    } else if (value->tag.constructed) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_STRUCTURE;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 3U && value->value_length == 1U) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_BOOL;
        result->bool_value = value->value_bytes[0] != 0U ? 1 : 0;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 5U && value->value_length <= sizeof(uint64_t)) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_INTEGER;
        result->integer_value = decode_signed_bytes(value->value_bytes, value->value_length);
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 6U && value->value_length <= sizeof(uint64_t)) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_UNSIGNED;
        result->unsigned_value = decode_unsigned_bytes(value->value_bytes, value->value_length);
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 7U && decode_mms_float32_value(value->value_bytes, value->value_length, &result->floating_value)) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_FLOAT;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 4U) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_BIT_STRING;
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 9U) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_OCTETS;
    } else if (unitlab_native_client_bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
        result->value_kind = UNITLAB_NATIVE_REPORT_VALUE_STRING;
    }
}

static void format_read_value_summary(const UnitLabMmsBerElement* value, UnitLabNativeLastReadResult* result)
{
    if (result == NULL) {
        return;
    }
    result->value_summary[0] = '\0';
    if (value == NULL || value->value_bytes == NULL || value->value_length == 0U) {
        snprintf(result->value_summary, sizeof(result->value_summary), "%s", "<empty>");
    } else if (result->value_kind == UNITLAB_NATIVE_REPORT_VALUE_BOOL) {
        snprintf(result->value_summary, sizeof(result->value_summary), "%s", result->bool_value ? "true" : "false");
    } else if (result->value_kind == UNITLAB_NATIVE_REPORT_VALUE_INTEGER) {
        snprintf(result->value_summary, sizeof(result->value_summary), "%lld", (long long)result->integer_value);
    } else if (result->value_kind == UNITLAB_NATIVE_REPORT_VALUE_UNSIGNED) {
        snprintf(result->value_summary, sizeof(result->value_summary), "%llu", (unsigned long long)result->unsigned_value);
    } else if (result->value_kind == UNITLAB_NATIVE_REPORT_VALUE_FLOAT) {
        snprintf(result->value_summary, sizeof(result->value_summary), "%.9g", result->floating_value);
    } else if (unitlab_native_client_bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
        size_t copy_length = value->value_length < sizeof(result->value_summary) - 1U ? value->value_length : sizeof(result->value_summary) - 1U;
        memcpy(result->value_summary, value->value_bytes, copy_length);
        result->value_summary[copy_length] = '\0';
    } else {
        snprintf(result->value_summary, sizeof(result->value_summary), "%s", "0x");
        append_hex_summary(result->value_summary, sizeof(result->value_summary), value->value_bytes, value->value_length);
    }
}

static void emit_structured_access_result_summary(size_t access_result_index, const UnitLabMmsBerElement* result)
{
    UnitLabMmsDiagnostic diagnostic;
    size_t offset = 0U;
    size_t field_index = 0U;
    size_t field_count;
    const char* rcb_kind;

    if (result == NULL
        || result->tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || !result->tag.constructed
        || result->tag.tag_number != 2U
        || result->value_bytes == NULL) {
        return;
    }
    field_count = count_constructed_children(result);
    if (field_count == 14U) {
        rcb_kind = "brcb";
    } else if (field_count == 11U) {
        rcb_kind = "urcb";
    } else {
        return;
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    printf("mms-summary: accessResult[%zu].rcb-kind=%s fields=%zu\n", access_result_index, rcb_kind, field_count);
    while (offset < result->value_length) {
        UnitLabMmsBerElement field;
        const char* field_name = field_count == 14U ? brcb_field_name(field_index) : urcb_field_name(field_index);
        size_t consumed = 0U;

        unitlab_mms_ber_element_init(&field);
        if (!unitlab_mms_ber_read(&field, &result->value_bytes[offset], result->value_length - offset, &consumed, &diagnostic) || consumed == 0U) {
            printf("mms-summary: accessResult[%zu].rcb-field[%zu]=decode-failed\n", access_result_index, field_index);
            fflush(stdout);
            return;
        }
        printf(
            "mms-summary: accessResult[%zu].rcb-field[%zu].%s=",
            access_result_index,
            field_index,
            field_name != NULL ? field_name : "unknown");
        print_data_value_summary(&field);
        printf("\n");
        offset += consumed;
        field_index++;
    }
    fflush(stdout);
}

static void emit_service_access_results(const UnitLabMmsPdu* pdu)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement outer;
    const uint8_t* list_bytes;
    size_t list_length;
    size_t consumed = 0U;
    size_t offset = 0U;
    size_t index = 0U;

    if (pdu == NULL || pdu->service_bytes == NULL || pdu->service_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    list_bytes = pdu->service_bytes;
    list_length = pdu->service_length;

    unitlab_mms_ber_element_init(&outer);
    if (unitlab_mms_ber_read(&outer, pdu->service_bytes, pdu->service_length, &consumed, &diagnostic)
        && consumed == pdu->service_length
        && outer.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        && outer.tag.constructed
        && outer.tag.tag_number == 1U) {
        list_bytes = outer.value_bytes;
        list_length = outer.value_length;
    }

    while (offset < list_length) {
        UnitLabMmsBerElement result;
        size_t result_consumed = 0U;
        unitlab_mms_ber_element_init(&result);
        if (!unitlab_mms_ber_read(&result, &list_bytes[offset], list_length - offset, &result_consumed, &diagnostic) || result_consumed == 0U) {
            printf("mms-summary: accessResult[%zu]=decode-failed\n", index);
            fflush(stdout);
            return;
        }
        if (result.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && result.tag.tag_number == 0U) {
            printf(
                "mms-summary: accessResult[%zu]=failure code=%u\n",
                index,
                (unsigned)decode_unsigned_bytes(result.value_bytes, result.value_length));
        } else {
            printf(
                "mms-summary: accessResult[%zu]=%s tag=%u length=%zu",
                index,
                access_result_label(&result),
                (unsigned)result.tag.tag_number,
                result.value_length);
            if (result.value_bytes != NULL && result.value_length > 0U) {
                if (unitlab_native_client_bytes_are_printable_ascii(result.value_bytes, result.value_length)) {
                    size_t printable_length = result.value_length < 96U ? result.value_length : 96U;
                    printf(" value-string=\"");
                    fwrite(result.value_bytes, 1U, printable_length, stdout);
                    if (result.value_length > printable_length) {
                        fputs("...", stdout);
                    }
                    printf("\"");
                } else if (result.value_length <= 4U && (result.tag.tag_number == 5U || result.tag.tag_number == 6U)) {
                    printf(" value-uint=%u", (unsigned)decode_unsigned_bytes(result.value_bytes, result.value_length));
                } else {
                    printf(" value-hex=");
                    print_hex_value(result.value_bytes, result.value_length);
                }
            }
            printf("\n");
            emit_structured_access_result_summary(index, &result);
        }
        fflush(stdout);
        offset += result_consumed;
        index++;
    }
    printf("mms-summary: accessResult-count=%zu\n", index);
    fflush(stdout);
}

static int store_latest_read_result(
    UnitLabNativeClientSessionState* session,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    const UnitLabMmsPdu* pdu)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement outer;
    UnitLabMmsBerElement result;
    const UnitLabNativeDiscoveredLeafRef* leaf_ref;
    const uint8_t* list_bytes;
    size_t list_length;
    size_t consumed = 0U;
    size_t result_consumed = 0U;

    if (session == NULL || domain_id == NULL || item_id == NULL || pdu == NULL
        || pdu->kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        || pdu->service_kind != UNITLAB_MMS_SERVICE_READ
        || pdu->service_bytes == NULL
        || pdu->service_length == 0U) {
        return 0;
    }

    memset(&session->last_read_result, 0, sizeof(session->last_read_result));
    session->has_last_read_result = 0;
    session->last_read_invoke_id = invoke_id;
    snprintf(session->last_read_result.object_reference, sizeof(session->last_read_result.object_reference), "%s/%s", domain_id, item_id);
    snprintf(session->last_read_result.display_reference, sizeof(session->last_read_result.display_reference), "%s", session->last_read_result.object_reference);
    leaf_ref = unitlab_native_client_session_find_leaf_ref(session, session->last_read_result.object_reference);
    if (leaf_ref != NULL && leaf_ref->display_reference[0] != '\0') {
        snprintf(session->last_read_result.display_reference, sizeof(session->last_read_result.display_reference), "%s", leaf_ref->display_reference);
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    list_bytes = pdu->service_bytes;
    list_length = pdu->service_length;
    unitlab_mms_ber_element_init(&outer);
    if (unitlab_mms_ber_read(&outer, pdu->service_bytes, pdu->service_length, &consumed, &diagnostic)
        && consumed == pdu->service_length
        && outer.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        && outer.tag.constructed
        && outer.tag.tag_number == 1U) {
        list_bytes = outer.value_bytes;
        list_length = outer.value_length;
    }

    unitlab_mms_ber_element_init(&result);
    if (!unitlab_mms_ber_read(&result, list_bytes, list_length, &result_consumed, &diagnostic) || result_consumed == 0U) {
        return 0;
    }
    if (result.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && result.tag.tag_number == 0U) {
        session->last_read_result.raw_tag_class = (uint8_t)result.tag.tag_class;
        session->last_read_result.raw_tag_number = (uint8_t)result.tag.tag_number;
        session->last_read_result.raw_value_length = result.value_length;
        session->last_read_result.access_failure = 1;
        session->last_read_result.access_failure_code = decode_unsigned_bytes(result.value_bytes, result.value_length);
        snprintf(session->last_read_result.value_summary, sizeof(session->last_read_result.value_summary), "access-failure:%u", (unsigned)session->last_read_result.access_failure_code);
    } else {
        copy_read_typed_value(&result, &session->last_read_result);
        format_read_value_summary(&result, &session->last_read_result);
    }
    session->has_last_read_result = 1;
    printf(
        "native-wire-client: read-summary invoke=%u object=%s display=%s status=%s kind=%s tag=%u length=%zu value=%s\n",
        (unsigned)invoke_id,
        session->last_read_result.object_reference,
        session->last_read_result.display_reference,
        session->last_read_result.access_failure ? "access-failure" : "success",
        session->last_read_result.access_failure ? "failure" : report_value_kind_label(session->last_read_result.value_kind),
        (unsigned)session->last_read_result.raw_tag_number,
        session->last_read_result.raw_value_length,
        session->last_read_result.value_summary[0] != '\0' ? session->last_read_result.value_summary : "<none>");
    fflush(stdout);
    return 1;
}

static int copy_printable_value(const UnitLabMmsBerElement* element, char* buffer, size_t buffer_size)
{
    size_t copy_length;
    if (element == NULL || buffer == NULL || buffer_size == 0U || element->value_bytes == NULL || !unitlab_native_client_bytes_are_printable_ascii(element->value_bytes, element->value_length)) {
        return 0;
    }
    copy_length = element->value_length < buffer_size - 1U ? element->value_length : buffer_size - 1U;
    memcpy(buffer, element->value_bytes, copy_length);
    buffer[copy_length] = '\0';
    return 1;
}

static void emit_gva_components_from_bytes(
    const uint8_t* bytes,
    size_t length,
    size_t depth,
    size_t* component_count,
    size_t* printed_count)
{
    UnitLabMmsDiagnostic diagnostic;
    size_t offset = 0U;

    if (bytes == NULL || component_count == NULL || printed_count == NULL || depth > 16U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    while (offset < length) {
        UnitLabMmsBerElement element;
        size_t consumed = 0U;
        unitlab_mms_ber_element_init(&element);
        if (!unitlab_mms_ber_read(&element, &bytes[offset], length - offset, &consumed, &diagnostic) || consumed == 0U) {
            break;
        }
        if (element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && element.tag.constructed && element.tag.tag_number == 16U) {
            UnitLabMmsBerElement first_child;
            size_t child_consumed = 0U;
            unitlab_mms_ber_element_init(&first_child);
            if (unitlab_mms_ber_read(&first_child, element.value_bytes, element.value_length, &child_consumed, &diagnostic)
                && first_child.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
                && !first_child.tag.constructed
                && first_child.tag.tag_number == 0U
                && unitlab_native_client_bytes_are_printable_ascii(first_child.value_bytes, first_child.value_length)) {
                if (*printed_count < 16U) {
                    printf("mms-summary: gva-component[%zu]=\"", *component_count);
                    fwrite(first_child.value_bytes, 1U, first_child.value_length, stdout);
                    printf("\"\n");
                    (*printed_count)++;
                }
                (*component_count)++;
            } else {
                emit_gva_components_from_bytes(element.value_bytes, element.value_length, depth + 1U, component_count, printed_count);
            }
        } else if (element.tag.constructed) {
            emit_gva_components_from_bytes(element.value_bytes, element.value_length, depth + 1U, component_count, printed_count);
        }
        offset += consumed;
    }
}

static void emit_get_variable_access_attributes_summary(const UnitLabMmsPdu* pdu)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement mms_deletable;
    size_t consumed = 0U;
    size_t component_count = 0U;
    size_t printed = 0U;

    if (pdu == NULL || pdu->service_bytes == NULL || pdu->service_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&mms_deletable);
    if (!unitlab_mms_ber_read(&mms_deletable, pdu->service_bytes, pdu->service_length, &consumed, &diagnostic) || consumed >= pdu->service_length) {
        return;
    }
    emit_gva_components_from_bytes(&pdu->service_bytes[consumed], pdu->service_length - consumed, 0U, &component_count, &printed);
    if (component_count > 0U) {
        printf("mms-summary: gva-component-count=%zu", component_count);
        if (component_count > printed) {
            printf(" printed=%zu", printed);
        }
        printf("\n");
        fflush(stdout);
    }
}

static void emit_get_named_variable_list_attributes_summary(const UnitLabMmsPdu* pdu)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement deletable;
    UnitLabMmsBerElement list;
    size_t consumed = 0U;
    size_t offset = 0U;
    size_t member_count = 0U;
    size_t printed = 0U;

    if (pdu == NULL || pdu->service_bytes == NULL || pdu->service_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&deletable);
    if (!unitlab_mms_ber_read(&deletable, pdu->service_bytes, pdu->service_length, &consumed, &diagnostic)) {
        return;
    }
    printf("mms-summary: nvl-deletable=%s\n", deletable.value_length > 0U && deletable.value_bytes[0] != 0U ? "true" : "false");
    unitlab_mms_ber_element_init(&list);
    if (!unitlab_mms_ber_read(&list, &pdu->service_bytes[consumed], pdu->service_length - consumed, &consumed, &diagnostic)
        || !(list.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && list.tag.constructed && list.tag.tag_number == 1U)) {
        return;
    }
    while (offset < list.value_length) {
        UnitLabMmsBerElement member;
        UnitLabMmsBerElement variable_spec;
        UnitLabMmsBerElement object_name;
        char domain[128U];
        char item[256U];
        size_t member_consumed = 0U;
        size_t nested_consumed = 0U;

        unitlab_mms_ber_element_init(&member);
        if (!unitlab_mms_ber_read(&member, &list.value_bytes[offset], list.value_length - offset, &member_consumed, &diagnostic) || member_consumed == 0U) {
            break;
        }
        if (printed < 16U
            && member.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
            && member.tag.constructed
            && member.tag.tag_number == 16U
            && unitlab_mms_ber_read(&variable_spec, member.value_bytes, member.value_length, &nested_consumed, &diagnostic)
            && variable_spec.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
            && variable_spec.tag.constructed
            && variable_spec.tag.tag_number == 0U
            && unitlab_mms_ber_read(&object_name, variable_spec.value_bytes, variable_spec.value_length, &nested_consumed, &diagnostic)
            && unitlab_native_client_decode_object_name_domain_item(&object_name, domain, sizeof(domain), item, sizeof(item))) {
            printf("mms-summary: nvl-member[%zu]=%s/%s\n", member_count, domain[0] != '\0' ? domain : "<vmd>", item);
            printed++;
        }
        offset += member_consumed;
        member_count++;
    }
    printf("mms-summary: nvl-member-count=%zu", member_count);
    if (member_count > printed) {
        printf(" printed=%zu", printed);
    }
    printf("\n");
    fflush(stdout);
}

static void emit_get_name_list_identifiers(const UnitLabMmsPdu* pdu)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement list_element;
    UnitLabMmsBerElement more_follows_element;
    size_t consumed = 0U;
    size_t offset = 0U;
    size_t index = 0U;
    size_t printed = 0U;

    if (pdu == NULL || pdu->service_bytes == NULL || pdu->service_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&list_element);
    if (!unitlab_mms_ber_read(&list_element, pdu->service_bytes, pdu->service_length, &consumed, &diagnostic)
        || list_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || list_element.tag.tag_number != 0U) {
        return;
    }

    while (offset < list_element.value_length) {
        UnitLabMmsBerElement identifier;
        size_t identifier_consumed = 0U;
        unitlab_mms_ber_element_init(&identifier);
        if (!unitlab_mms_ber_read(&identifier, &list_element.value_bytes[offset], list_element.value_length - offset, &identifier_consumed, &diagnostic) || identifier_consumed == 0U) {
            break;
        }
        if (printed < 16U && identifier.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && identifier.tag.tag_number == 26U) {
            printf("mms-summary: identifier[%zu]=\"", index);
            fwrite(identifier.value_bytes, 1U, identifier.value_length, stdout);
            printf("\"\n");
            printed++;
        }
        offset += identifier_consumed;
        index++;
    }
    printf("mms-summary: identifier-count=%zu", index);
    if (index > printed) {
        printf(" printed=%zu", printed);
    }

    if (consumed < pdu->service_length) {
        size_t more_follows_consumed = 0U;
        unitlab_mms_ber_element_init(&more_follows_element);
        if (unitlab_mms_ber_read(&more_follows_element, &pdu->service_bytes[consumed], pdu->service_length - consumed, &more_follows_consumed, &diagnostic)
            && more_follows_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
            && more_follows_element.tag.tag_number == 1U
            && more_follows_element.value_length > 0U) {
            printf(" moreFollows=%s", more_follows_element.value_bytes[0] != 0U ? "true" : "false");
        }
    }
    printf("\n");
    fflush(stdout);
}

static int report_opt_bit_enabled(const uint8_t* bytes, size_t length, unsigned bit_index)
{
    size_t data_index = 1U + (bit_index / 8U);
    uint8_t mask = (uint8_t)(0x80U >> (bit_index % 8U));
    if (bytes == NULL || length <= data_index) {
        return 0;
    }
    return (bytes[data_index] & mask) != 0U;
}

static int report_bit_string_bit_enabled(const uint8_t* bytes, size_t length, size_t bit_index)
{
    size_t data_index = 1U + (bit_index / 8U);
    uint8_t mask = (uint8_t)(0x80U >> (bit_index % 8U));

    if (bytes == NULL || length <= data_index) {
        return 0;
    }
    return (bytes[data_index] & mask) != 0U;
}

static void print_report_value_summary(const UnitLabMmsBerElement* value)
{
    print_data_value_summary(value);
}

static int read_next_report_value(
    const uint8_t* bytes,
    size_t length,
    size_t* offset,
    UnitLabMmsBerElement* element,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t consumed = 0U;
    if (bytes == NULL || offset == NULL || element == NULL || *offset >= length) {
        return 0;
    }
    unitlab_mms_ber_element_init(element);
    if (!unitlab_mms_ber_read(element, &bytes[*offset], length - *offset, &consumed, diagnostic) || consumed == 0U) {
        return 0;
    }
    *offset += consumed;
    return 1;
}

static void emit_information_report_summary(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    const UnitLabMmsPdu* pdu)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement list_name_wrapper;
    UnitLabMmsBerElement list_name;
    UnitLabMmsBerElement values_wrapper;
    UnitLabMmsBerElement value;
    size_t consumed = 0U;
    size_t inner_consumed = 0U;
    size_t offset = 0U;
    size_t values_offset = 0U;
    size_t data_ref_count = 0U;
    size_t value_count = 0U;
    size_t reason_count = 0U;
    size_t matched_data_ref_count = 0U;
    size_t dataset_mismatch_count = 0U;
    size_t missing_value_count = 0U;
    size_t extra_value_count = 0U;
    size_t missing_reason_count = 0U;
    size_t extra_reason_count = 0U;
    size_t unsupported_value_count = 0U;
    const uint8_t* opt_flds = NULL;
    size_t opt_flds_length = 0U;
    int has_data_reference = 0;
    int has_reason = 0;
    int report_data_set_discovered = 0;
    const uint8_t* inclusion_bytes = NULL;
    size_t inclusion_length = 0U;
    char report_rpt_id[160U] = { 0 };
    uint32_t report_sub_sequence_number = 0U;
    int has_sub_sequence_number = 0;

    if (pdu == NULL || pdu->service_bytes == NULL || pdu->service_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&list_name_wrapper);
    if (!unitlab_mms_ber_read(&list_name_wrapper, pdu->service_bytes, pdu->service_length, &consumed, &diagnostic)
        || list_name_wrapper.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || !list_name_wrapper.tag.constructed
        || list_name_wrapper.tag.tag_number != 1U) {
        return;
    }
    unitlab_mms_ber_element_init(&list_name);
    if (unitlab_mms_ber_read(&list_name, list_name_wrapper.value_bytes, list_name_wrapper.value_length, &inner_consumed, &diagnostic)
        && list_name.value_bytes != NULL
        && unitlab_native_client_bytes_are_printable_ascii(list_name.value_bytes, list_name.value_length)) {
        printf("mms-summary: report.variable-list=");
        print_report_value_summary(&list_name);
        printf("\n");
    }
    offset = consumed;
    unitlab_mms_ber_element_init(&values_wrapper);
    if (!unitlab_mms_ber_read(&values_wrapper, &pdu->service_bytes[offset], pdu->service_length - offset, &consumed, &diagnostic)
        || values_wrapper.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || !values_wrapper.tag.constructed
        || values_wrapper.tag.tag_number != 0U) {
        return;
    }

    if (!read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        return;
    }
    (void)copy_printable_value(&value, report_rpt_id, sizeof(report_rpt_id));
    printf("mms-summary: report.RptID=");
    print_report_value_summary(&value);
    printf("\n");

    if (!read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        return;
    }
    opt_flds = value.value_bytes;
    opt_flds_length = value.value_length;
    has_data_reference = report_opt_bit_enabled(opt_flds, opt_flds_length, 5U);
    has_reason = report_opt_bit_enabled(opt_flds, opt_flds_length, 6U);
    printf("mms-summary: report.OptFlds=");
    print_report_value_summary(&value);
    printf(" dataRef=%s reason=%s\n", has_data_reference ? "true" : "false", has_reason ? "true" : "false");

    if (report_opt_bit_enabled(opt_flds, opt_flds_length, 1U) && read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        UnitLabNativeReportSequenceDisposition sequence_disposition;
        uint32_t report_sequence_number;

        printf("mms-summary: report.SqNum=");
        print_report_value_summary(&value);
        printf("\n");
        report_sequence_number = decode_unsigned_bytes(value.value_bytes, value.value_length);
        {
            size_t sub_sequence_checkpoint = values_offset;
            UnitLabMmsBerElement sub_sequence_value;
            unitlab_mms_ber_element_init(&sub_sequence_value);
            if (read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &sub_sequence_value, &diagnostic)
                && sub_sequence_value.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
                && !sub_sequence_value.tag.constructed
                && sub_sequence_value.tag.tag_number == 7U) {
                report_sub_sequence_number = decode_unsigned_bytes(sub_sequence_value.value_bytes, sub_sequence_value.value_length);
                has_sub_sequence_number = 1;
                printf("mms-summary: report.SubSqNum=");
                print_report_value_summary(&sub_sequence_value);
                printf("\n");
            } else {
                values_offset = sub_sequence_checkpoint;
            }
        }
        sequence_disposition = unitlab_native_client_session_observe_report_sequence(
            session,
            session_runtime != NULL ? session_runtime->identity.connection_generation : 0U,
            report_sequence_number,
            has_sub_sequence_number,
            report_sub_sequence_number);
        if (sequence_disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_DUPLICATE || sequence_disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_OUT_OF_ORDER) {
            if (session_runtime != NULL) {
                unitlab_native_session_runtime_mark_report_health_stale(
                    session_runtime,
                    session->subscription_model.report_health_reason);
            }
            printf(
                "mms-summary: report.diagnostic code=%s sqNum=%u subSqNum=%u lastSqNum=%u lastSubSqNum=%u generation=%llu\n",
                sequence_disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_DUPLICATE ? "DUPLICATE_REPORT_SEQUENCE" : "OUT_OF_ORDER_REPORT_SEQUENCE",
                (unsigned)report_sequence_number,
                (unsigned)report_sub_sequence_number,
                (unsigned)session->subscription_model.last_report_sequence_number,
                (unsigned)session->subscription_model.last_report_sub_sequence_number,
                (unsigned long long)session->subscription_model.last_report_sequence_generation);
            emit_subscription_summary(session, report_sequence_disposition_label(sequence_disposition));
            return;
        }
        if (sequence_disposition == UNITLAB_NATIVE_REPORT_SEQUENCE_GAP) {
            if (session_runtime != NULL) {
                unitlab_native_session_runtime_mark_report_health_stale(
                    session_runtime,
                    session->subscription_model.report_health_reason);
            }
            printf(
                "mms-summary: report.diagnostic code=REPORT_SEQUENCE_GAP sqNum=%u subSqNum=%u lastSqNum=%u lastSubSqNum=%u missing=%llu generation=%llu\n",
                (unsigned)report_sequence_number,
                (unsigned)report_sub_sequence_number,
                (unsigned)session->subscription_model.last_report_sequence_number,
                (unsigned)session->subscription_model.last_report_sub_sequence_number,
                (unsigned long long)session->subscription_model.report_sequence_missing_count,
                (unsigned long long)session->subscription_model.last_report_sequence_generation);
        }
    }
    unitlab_native_client_session_reset_last_report(session);
    snprintf(session->discovered_model.last_report_rpt_id, sizeof(session->discovered_model.last_report_rpt_id), "%s", report_rpt_id);
    if (report_opt_bit_enabled(opt_flds, opt_flds_length, 2U) && read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        printf("mms-summary: report.TimeOfEntry=");
        print_report_value_summary(&value);
        printf("\n");
    }
    if (report_opt_bit_enabled(opt_flds, opt_flds_length, 4U) && read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        (void)copy_printable_value(&value, session->discovered_model.last_report_data_set, sizeof(session->discovered_model.last_report_data_set));
        report_data_set_discovered = unitlab_native_client_session_data_set_index_by_reference(session, session->discovered_model.last_report_data_set, NULL);
        printf("mms-summary: report.DatSet=");
        print_report_value_summary(&value);
        printf(" discovered=%s\n", report_data_set_discovered ? "true" : "false");
    }
    if (report_opt_bit_enabled(opt_flds, opt_flds_length, 3U) && read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        printf("mms-summary: report.BufOvfl=");
        print_report_value_summary(&value);
        printf("\n");
    }
    if (report_opt_bit_enabled(opt_flds, opt_flds_length, 7U) && read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        printf("mms-summary: report.EntryID=");
        print_report_value_summary(&value);
        printf("\n");
    }
    if (report_opt_bit_enabled(opt_flds, opt_flds_length, 8U) && read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        printf("mms-summary: report.ConfRev=");
        print_report_value_summary(&value);
        printf("\n");
    }
    if (!read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
        return;
    }
    inclusion_bytes = value.value_bytes;
    inclusion_length = value.value_length;
    printf("mms-summary: report.inclusion=");
    print_report_value_summary(&value);
    printf("\n");

    if (!has_data_reference && report_data_set_discovered) {
        size_t member_index = 0U;
        size_t mapped_index = 0U;
        const char* member_reference = unitlab_native_client_session_data_set_member_at(session, session->discovered_model.last_report_data_set, member_index);
        while (member_reference != NULL) {
            if (report_bit_string_bit_enabled(inclusion_bytes, inclusion_length, member_index)) {
                UnitLabNativeLastReportEntry* entry = unitlab_native_client_session_append_last_report_entry(session, member_reference, 1, member_index);
                if (entry != NULL) {
                    matched_data_ref_count++;
                    printf("mms-summary: report.datasetRef[%zu]=%s dataset-index=%zu display-ref=%s\n", mapped_index, member_reference, member_index, entry->display_reference);
                }
                mapped_index++;
            }
            member_index++;
            member_reference = unitlab_native_client_session_data_set_member_at(session, session->discovered_model.last_report_data_set, member_index);
        }
    }

    if (has_data_reference) {
        while (values_offset < values_wrapper.value_length) {
            UnitLabMmsBerElement next;
            size_t checkpoint = values_offset;
            if (!read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &next, &diagnostic)) {
                return;
            }
            if (!(next.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
                && !next.tag.constructed
                && next.tag.tag_number == 10U
                && next.value_bytes != NULL
                && unitlab_native_client_bytes_are_printable_ascii(next.value_bytes, next.value_length))) {
                values_offset = checkpoint;
                break;
            }
            {
                char reference[384U];
                int copied = copy_printable_value(&next, reference, sizeof(reference));
                int global_matched = copied && unitlab_native_client_session_data_set_member_exists(session, reference);
                int data_set_matched = copied && unitlab_native_client_session_data_set_contains_member(session, session->discovered_model.last_report_data_set, reference);
                UnitLabNativeLastReportEntry* entry = NULL;
                if (data_set_matched) {
                    matched_data_ref_count++;
                } else if (copied) {
                    dataset_mismatch_count++;
                }
                if (copied) {
                    entry = unitlab_native_client_session_append_last_report_entry(session, reference, data_set_matched, data_ref_count);
                }
                printf("mms-summary: report.dataRef[%zu]=", data_ref_count);
                print_report_value_summary(&next);
                printf(" discovered-match=%s dataset-match=%s", global_matched ? "true" : "false", data_set_matched ? "true" : "false");
                if (entry != NULL) {
                    printf(" display-ref=%s", entry->display_reference);
                }
                printf("\n");
            }
            data_ref_count++;
        }
    }

    while (values_offset < values_wrapper.value_length) {
        UnitLabMmsBerElement next;
        size_t checkpoint = values_offset;
        if (!read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &next, &diagnostic)) {
            return;
        }
        if (has_reason
            && next.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
            && !next.tag.constructed
            && next.tag.tag_number == 4U
            && next.value_length == 2U) {
            values_offset = checkpoint;
            break;
        }
        if (value_count < session->last_report_entry_count) {
            copy_data_value_summary(&next, session->last_report_entries[value_count].value_summary, sizeof(session->last_report_entries[value_count].value_summary));
            copy_report_typed_value(&next, &session->last_report_entries[value_count]);
            if (session->last_report_entries[value_count].value_kind == UNITLAB_NATIVE_REPORT_VALUE_UNSUPPORTED) {
                unsupported_value_count++;
            }
        }
        printf("mms-summary: report.value[%zu]=", value_count);
        print_report_value_summary(&next);
        if (value_count < session->last_report_entry_count) {
            printf(" ref=%s kind=%s", session->last_report_entries[value_count].display_reference, report_value_kind_label(session->last_report_entries[value_count].value_kind));
            if (session->last_report_entries[value_count].quality_validity[0] != '\0') {
                printf(" quality=0x%04x validity=%s", (unsigned)session->last_report_entries[value_count].quality_code, session->last_report_entries[value_count].quality_validity);
            }
        }
        printf("\n");
        value_count++;
    }

    if (has_reason) {
        while (values_offset < values_wrapper.value_length) {
            if (!read_next_report_value(values_wrapper.value_bytes, values_wrapper.value_length, &values_offset, &value, &diagnostic)) {
                return;
            }
            if (reason_count < session->last_report_entry_count) {
                copy_data_value_summary(&value, session->last_report_entries[reason_count].reason_summary, sizeof(session->last_report_entries[reason_count].reason_summary));
                copy_report_reason_metadata(&value, &session->last_report_entries[reason_count]);
            }
            printf("mms-summary: report.reason[%zu]=", reason_count);
            print_report_value_summary(&value);
            if (reason_count < session->last_report_entry_count) {
                printf(" ref=%s reason-code=0x%04x reason=%s", session->last_report_entries[reason_count].display_reference, (unsigned)session->last_report_entries[reason_count].reason_code, session->last_report_entries[reason_count].reason_labels[0] != '\0' ? session->last_report_entries[reason_count].reason_labels : "unknown");
            }
            printf("\n");
            reason_count++;
        }
    }
    if (!has_data_reference && !report_data_set_discovered) {
        printf("mms-summary: report.diagnostic code=DATASET_NOT_DISCOVERED datSet=%s message=report cannot be mapped without dataRef or discovered DatSet\n", session->discovered_model.last_report_data_set[0] != '\0' ? session->discovered_model.last_report_data_set : "<none>");
    }
    if (session->last_report_entry_count > value_count) {
        missing_value_count = session->last_report_entry_count - value_count;
        printf("mms-summary: report.diagnostic code=MISSING_REPORT_VALUES mapped-entry-count=%zu value-count=%zu missing=%zu\n", session->last_report_entry_count, value_count, missing_value_count);
    } else if (value_count > session->last_report_entry_count) {
        extra_value_count = value_count - session->last_report_entry_count;
        printf("mms-summary: report.diagnostic code=EXTRA_REPORT_VALUES mapped-entry-count=%zu value-count=%zu extra=%zu\n", session->last_report_entry_count, value_count, extra_value_count);
    }
    if (has_reason) {
        if (session->last_report_entry_count > reason_count) {
            missing_reason_count = session->last_report_entry_count - reason_count;
            printf("mms-summary: report.diagnostic code=MISSING_REPORT_REASONS mapped-entry-count=%zu reason-count=%zu missing=%zu\n", session->last_report_entry_count, reason_count, missing_reason_count);
        } else if (reason_count > session->last_report_entry_count) {
            extra_reason_count = reason_count - session->last_report_entry_count;
            printf("mms-summary: report.diagnostic code=EXTRA_REPORT_REASONS mapped-entry-count=%zu reason-count=%zu extra=%zu\n", session->last_report_entry_count, reason_count, extra_reason_count);
        }
    }
    if (dataset_mismatch_count > 0U) {
        printf("mms-summary: report.diagnostic code=DATASET_MEMBER_MISMATCH datSet=%s mismatches=%zu dataRef-count=%zu\n", session->discovered_model.last_report_data_set[0] != '\0' ? session->discovered_model.last_report_data_set : "<none>", dataset_mismatch_count, data_ref_count);
    }
    if (unsupported_value_count > 0U) {
        printf("mms-summary: report.diagnostic code=UNSUPPORTED_REPORT_VALUES unsupported=%zu mapped-entry-count=%zu\n", unsupported_value_count, session->last_report_entry_count);
    }
    session->discovered_model.last_report_data_ref_count = data_ref_count;
    session->discovered_model.last_report_value_count = value_count;
    session->discovered_model.last_report_reason_count = reason_count;
    session->discovered_model.last_report_matched_data_ref_count = matched_data_ref_count;
    session->discovered_model.last_report_dataset_mismatch_count = dataset_mismatch_count;
    session->discovered_model.last_report_missing_value_count = missing_value_count;
    session->discovered_model.last_report_extra_value_count = extra_value_count;
    session->discovered_model.last_report_missing_reason_count = missing_reason_count;
    session->discovered_model.last_report_extra_reason_count = extra_reason_count;
    session->discovered_model.last_report_unsupported_value_count = unsupported_value_count;
    session->subscription_model.last_report_received = 1;
    emit_report_entry_contract(session);
    printf("mms-summary: report.dataRef-count=%zu value-count=%zu reason-count=%zu mapped-entry-count=%zu dataset-mismatch-count=%zu missing-value-count=%zu extra-value-count=%zu missing-reason-count=%zu extra-reason-count=%zu unsupported-value-count=%zu\n", data_ref_count, value_count, reason_count, session->last_report_entry_count, dataset_mismatch_count, missing_value_count, extra_value_count, missing_reason_count, extra_reason_count, unsupported_value_count);
    emit_discovered_model_summary(session, "report");
    emit_subscription_summary(session, "report");
    fflush(stdout);
}

static int emit_mms_frame_summary(UnitLabNativeClientSessionState* session, UnitLabNativeSessionRuntime* session_runtime, const uint8_t* frame, size_t frame_length)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed = 0U;
    int report_received = 0;

    if (session == NULL || frame == NULL || frame_length == 0U) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, frame, frame_length, &consumed, &diagnostic)) {
        return 0;
    }
    if (association_frame.presentation.payload_bytes == NULL || association_frame.presentation.payload_length == 0U) {
        return 0;
    }
    unitlab_mms_pdu_init(&pdu);
    if (!unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed, &diagnostic)) {
        return 0;
    }
    printf(
        "mms-summary: pdu=%s invoke=%u service=%s serviceTag=%u serviceLength=%zu\n",
        pdu_kind_label(pdu.kind),
        (unsigned)(pdu.has_invoke_id ? pdu.invoke_id : 0U),
        pdu.has_service ? service_kind_label(pdu.service_kind) : "none",
        (unsigned)(pdu.has_service ? pdu.service_tag.tag_number : 0U),
        pdu.service_length);
    fflush(stdout);
    if (pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        && (pdu.service_kind == UNITLAB_MMS_SERVICE_READ || pdu.service_kind == UNITLAB_MMS_SERVICE_WRITE)) {
        emit_service_access_results(&pdu);
    }
    if (pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        && pdu.service_kind == UNITLAB_MMS_SERVICE_READ
        && pdu.has_invoke_id
        && session->has_pending_read
        && session->pending_read_invoke_id == pdu.invoke_id) {
        (void)store_latest_read_result(session, session->pending_read_domain, session->pending_read_item, pdu.invoke_id, &pdu);
    }
    if (pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE && pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAME_LIST) {
        emit_get_name_list_identifiers(&pdu);
    }
    if (pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE && pdu.service_kind == UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES) {
        emit_get_variable_access_attributes_summary(&pdu);
    }
    if (pdu.kind == UNITLAB_MMS_PDU_CONFIRMED_RESPONSE && pdu.service_kind == UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
        emit_get_named_variable_list_attributes_summary(&pdu);
    }
    if (pdu.kind == UNITLAB_MMS_PDU_UNCONFIRMED && pdu.service_kind == UNITLAB_MMS_SERVICE_INFORMATION_REPORT) {
        emit_information_report_summary(session, session_runtime, &pdu);
        report_received = 1;
    }
    return report_received;
}

int unitlab_native_wire_client_decode_frame_summary(
    UnitLabNativeClientSessionState* session,
    const uint8_t* frame,
    size_t frame_length)
{
    if (session == NULL || frame == NULL || frame_length == 0U) {
        return 0;
    }
    emit_mms_frame_summary(session, NULL, frame, frame_length);
    return session->subscription_model.last_report_received ? 1 : 0;
}

static void worker_copy_error(
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size,
    const char* code,
    const char* message)
{
    if (error_code != NULL && error_code_size > 0U) {
        snprintf(error_code, error_code_size, "%s", code != NULL && code[0] != '\0' ? code : "NATIVE_WIRE_CLIENT_WORKER_FAILED");
    }
    if (error_message != NULL && error_message_size > 0U) {
        snprintf(error_message, error_message_size, "%s", message != NULL && message[0] != '\0' ? message : "");
    }
}

static void worker_close_transport(UnitLabNativeWireClientWorkerContext* context)
{
    if (context == NULL) {
        return;
    }
    if (context->data_fd >= 0) {
        close(context->data_fd);
        context->data_fd = -1;
    }
    if (context->control_fd >= 0) {
        close(context->control_fd);
        context->control_fd = -1;
    }
}

static int read_tpkt_frame_status_with_timeout(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length, int timeout_ms);
static int read_tpkt_frame(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length);
static int validate_association_response_frame(const uint8_t* frame, size_t frame_length, UnitLabIedModelLoadResult* result);
static int discovery_get_name_list_step_adapter(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* node_id,
    const char* continue_after,
    uint32_t invoke_id);
static int discovery_read_step_adapter(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id);
static int discovery_attributes_step_adapter(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list);
static int native_wire_client_preflight_selected_rcb(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* rcb_item,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);
static int emit_discovered_rcb_bool_step(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* rcb_item,
    const char* field_name,
    uint8_t value,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);
static int native_wire_client_subscription_is_unbuffered_rcb(const UnitLabNativeClientSessionState* session);
static int native_wire_client_cleanup_selected_subscription(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);

void unitlab_native_wire_client_worker_context_init(
    UnitLabNativeWireClientWorkerContext* context,
    const UnitLabIedServerConfig* config)
{
    if (context == NULL) {
        return;
    }
    memset(context, 0, sizeof(*context));
    context->config = config;
    context->data_fd = -1;
    context->control_fd = -1;
    unitlab_native_client_session_reset(&context->session);
}

void unitlab_native_wire_client_worker_context_reset(UnitLabNativeWireClientWorkerContext* context)
{
    if (context == NULL) {
        return;
    }
    worker_close_transport(context);
    unitlab_native_client_session_reset(&context->session);
    context->config = NULL;
}

int unitlab_native_wire_client_worker_connect(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);
int unitlab_native_wire_client_worker_discover(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);
int unitlab_native_wire_client_worker_subscribe(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);
int unitlab_native_wire_client_worker_reconnect(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size);

void unitlab_native_wire_client_worker_default_handlers(UnitLabNativeSessionWorkerHandlers* handlers)
{
    if (handlers == NULL) {
        return;
    }
    memset(handlers, 0, sizeof(*handlers));
    handlers->connect = unitlab_native_wire_client_worker_connect;
    handlers->discover = unitlab_native_wire_client_worker_discover;
    handlers->subscribe = unitlab_native_wire_client_worker_subscribe;
    handlers->reconnect = unitlab_native_wire_client_worker_reconnect;
}

static const UnitLabNativeDiscoveredRcb* worker_select_rcb(
    UnitLabNativeWireClientWorkerContext* context,
    const UnitLabNativeSessionRuntime* runtime,
    size_t* index_out)
{
    const UnitLabNativeDiscoveredRcb* selected_rcb = NULL;
    size_t selected_index = (size_t)-1;

    if (context == NULL) {
        return NULL;
    }
    if (runtime != NULL && runtime->identity.rcb_key[0] != '\0') {
        selected_rcb = unitlab_native_client_session_find_discovered_rcb(&context->session, NULL, runtime->identity.rcb_key);
        if (selected_rcb != NULL) {
            for (size_t index = 0U; index < context->session.discovered_rcb_count; index++) {
                if (&context->session.discovered_rcbs[index] == selected_rcb) {
                    selected_index = index;
                    break;
                }
            }
        }
    }
    if (selected_rcb == NULL && context->session.subscription_model.selected_rcb_index < context->session.discovered_rcb_count) {
        selected_index = context->session.subscription_model.selected_rcb_index;
        selected_rcb = unitlab_native_client_session_discovered_rcb_at(&context->session, selected_index);
    }
    if (selected_rcb == NULL && context->session.discovered_rcb_count > 0U) {
        selected_index = 0U;
        selected_rcb = unitlab_native_client_session_discovered_rcb_at(&context->session, 0U);
    }
    if (index_out != NULL) {
        *index_out = selected_index;
    }
    return selected_rcb;
}

static int worker_finish_association_response(
    const uint8_t* frame,
    size_t frame_length,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    UnitLabIedModelLoadResult result;

    memset(&result, 0, sizeof(result));
    if (validate_association_response_frame(frame, frame_length, &result)) {
        return 1;
    }
    worker_copy_error(error_code, error_code_size, error_message, error_message_size, result.code, result.message);
    return 0;
}

int unitlab_native_wire_client_worker_connect(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    UnitLabNativeWireClientWorkerContext* context = (UnitLabNativeWireClientWorkerContext*)user_data;
    uint8_t frame[4096U];
    uint8_t association_request[4096U];
    size_t encoded_length = 0U;
    size_t association_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    if (context == NULL || context->config == NULL || runtime == NULL) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_WORKER_INVALID", "Native wire client worker connect requires a configured context and runtime.");
        return 0;
    }
    worker_close_transport(context);
    unitlab_mms_diagnostic_clear(&diagnostic);
    context->data_fd = connect_socket(context->config->bind_address, context->config->port);
    if (context->data_fd < 0) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_CONNECT_FAILED", "Native wire client could not connect to the data endpoint.");
        return 0;
    }
    if (context->config->control_port > 0) {
        context->control_fd = connect_socket(context->config->bind_address, context->config->control_port);
        if (context->control_fd < 0) {
            worker_close_transport(context);
            worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_CONTROL_CONNECT_FAILED", "Native wire client could not connect to the control endpoint.");
            return 0;
        }
    }
    if (!unitlab_mms_build_cotp_connect_request_frame(frame, sizeof(frame), &encoded_length, &diagnostic)) {
        worker_close_transport(context);
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        return 0;
    }
    if (!send_all(context->data_fd, frame, encoded_length) || !read_tpkt_frame(context->data_fd, frame, sizeof(frame), &encoded_length)) {
        worker_close_transport(context);
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_COTP_EXCHANGE_FAILED", "Native wire client could not complete the COTP handshake.");
        return 0;
    }
    if (!unitlab_mms_build_live_wire_association_request_frame(association_request, sizeof(association_request), &association_length, &diagnostic)) {
        worker_close_transport(context);
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        return 0;
    }
    if (!send_all(context->data_fd, association_request, association_length)) {
        worker_close_transport(context);
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_ASSOCIATION_SEND_FAILED", "Native wire client could not send the association request.");
        return 0;
    }
    {
        int association_read_status = read_tpkt_frame_status_with_timeout(context->data_fd, association_request, sizeof(association_request), &association_length, 5000);
        if (association_read_status != 1) {
            worker_close_transport(context);
            worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_ASSOCIATION_READ_FAILED", "Native wire client could not read the association response frame.");
            return 0;
        }
    }
    if (!worker_finish_association_response(association_request, association_length, error_code, error_code_size, error_message, error_message_size)) {
        worker_close_transport(context);
        return 0;
    }
    return 1;
}

int unitlab_native_wire_client_worker_discover(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    UnitLabNativeWireClientWorkerContext* context = (UnitLabNativeWireClientWorkerContext*)user_data;
    uint8_t scratch[65535U];
    uint8_t read_request[65535U];
    uint8_t report_frame[65535U];
    uint8_t frame[65535U];
    size_t report_length = 0U;
    uint32_t invoke_id = 0U;
    uint32_t next_invoke_id = 0U;
    UnitLabNativeDiscoveryIo discovery_io;
    UnitLabNativeDiscoverySnapshot discovery_snapshot;
    char snapshot_endpoint[40U];
    char snapshot_source[40U];
    UnitLabMmsDiagnostic diagnostic;

    if (context == NULL || runtime == NULL) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_WORKER_INVALID", "Native wire client worker discover requires a configured context and runtime.");
        return 0;
    }
    if (context->data_fd < 0) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", "Native wire client cannot discover without an associated transport.");
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    invoke_id = unitlab_native_client_session_reserve_invoke_id(&context->session);
    if (invoke_id == 0U || invoke_id > UINT32_MAX - UNITLAB_NATIVE_DISCOVERY_MAX_INVOKE_SPAN) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_DISCOVER_INVOKE_INVALID", "Native wire client discover invokeBase is invalid.");
        return 0;
    }
    next_invoke_id = context->session.next_invoke_id;
    memset(&discovery_io, 0, sizeof(discovery_io));
    discovery_io.data_fd = context->data_fd;
    discovery_io.scratch = scratch;
    discovery_io.scratch_length = sizeof(scratch);
    discovery_io.request = read_request;
    discovery_io.request_length = sizeof(read_request);
    discovery_io.response = report_frame;
    discovery_io.response_length = sizeof(report_frame);
    discovery_io.encoded_response_length = &report_length;
    discovery_io.text_buffer = frame;
    discovery_io.text_buffer_length = sizeof(frame);
    discovery_io.diagnostic = &diagnostic;
    discovery_io.get_name_list_step = discovery_get_name_list_step_adapter;
    discovery_io.read_step = discovery_read_step_adapter;
    discovery_io.attributes_step = discovery_attributes_step_adapter;
    discovery_io.emit_model_summary = emit_discovered_model_summary;
    if (!unitlab_native_client_run_discover_root_sequence(&context->session, &discovery_io, invoke_id, &next_invoke_id)) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message[0] != '\0' ? diagnostic.message : "Native wire client discovery sequence failed.");
        return 0;
    }
    memset(&discovery_snapshot, 0, sizeof(discovery_snapshot));
    native_wire_copy_text(snapshot_endpoint, sizeof(snapshot_endpoint), runtime->identity.endpoint_id[0] != '\0' ? runtime->identity.endpoint_id : "native-wire-client");
    native_wire_copy_text(snapshot_source, sizeof(snapshot_source), context->session.discovered_model.domain[0] != '\0' ? context->session.discovered_model.domain : runtime->identity.endpoint_id);
    snprintf(discovery_snapshot.snapshot_id, sizeof(discovery_snapshot.snapshot_id), "%s:%llu:%zu", snapshot_endpoint, (unsigned long long)runtime->identity.connection_generation, context->session.discovered_model.logical_device_count + context->session.discovered_model.logical_node_count + context->session.discovered_model.data_set_count + context->session.discovered_model.brcb_count);
    native_wire_copy_text(discovery_snapshot.endpoint_id, sizeof(discovery_snapshot.endpoint_id), runtime->identity.endpoint_id[0] != '\0' ? runtime->identity.endpoint_id : "native-wire-client");
    native_wire_copy_text(discovery_snapshot.device_key, sizeof(discovery_snapshot.device_key), context->session.discovered_model.domain[0] != '\0' ? context->session.discovered_model.domain : runtime->identity.device_key);
    native_wire_copy_text(discovery_snapshot.source_hash, sizeof(discovery_snapshot.source_hash), snapshot_source);
    discovery_snapshot.created_at_ms = native_wire_now_ms();
    discovery_snapshot.logical_device_count = context->session.discovered_model.logical_device_count;
    discovery_snapshot.logical_node_count = context->session.discovered_model.logical_node_count;
    discovery_snapshot.data_set_count = context->session.discovered_model.data_set_count;
    discovery_snapshot.data_set_member_count = context->session.discovered_model.data_set_member_count;
    discovery_snapshot.report_control_count = context->session.discovered_model.brcb_count;
    discovery_snapshot.signal_count = context->session.discovered_model.data_component_count;
    unitlab_native_session_runtime_update_discovery_snapshot(runtime, &discovery_snapshot);
    if (context->session.discovered_model.domain[0] != '\0') {
        native_wire_copy_text(runtime->identity.device_key, sizeof(runtime->identity.device_key), context->session.discovered_model.domain);
    }
    return 1;
}

int unitlab_native_wire_client_worker_subscribe(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    UnitLabNativeWireClientWorkerContext* context = (UnitLabNativeWireClientWorkerContext*)user_data;
    const UnitLabNativeDiscoveredRcb* selected_rcb;
    size_t selected_rcb_index = 0U;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t scratch[65535U];
    uint8_t read_request[65535U];
    uint8_t report_frame[65535U];
    uint8_t frame[65535U];
    size_t report_length = 0U;
    uint32_t preflight_invoke_id;
    uint32_t invoke_id;

    if (context == NULL || runtime == NULL) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_WORKER_INVALID", "Native wire client worker subscribe requires a configured context and runtime.");
        return 0;
    }
    if (context->data_fd < 0) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_RPTENA_FAILED", "Native wire client cannot subscribe without an associated transport.");
        return 0;
    }
    selected_rcb = worker_select_rcb(context, runtime, &selected_rcb_index);
    if (selected_rcb == NULL) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_RPTENA_NO_DISCOVERED_RCB", "Native wire client could not select a discovered report control block.");
        return 0;
    }
    preflight_invoke_id = unitlab_native_client_session_reserve_invoke_id(&context->session);
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (!native_wire_client_preflight_selected_rcb(&context->session, context->data_fd, "rptena-preflight", selected_rcb->domain, selected_rcb->item, preflight_invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_RPTENA_PREFLIGHT_FAILED", diagnostic.message);
        return 0;
    }
    if (native_wire_client_subscription_is_unbuffered_rcb(&context->session)) {
        invoke_id = unitlab_native_client_session_reserve_invoke_id(&context->session);
        if (!emit_discovered_rcb_bool_step(&context->session, runtime, context->data_fd, "reserve", selected_rcb->domain, selected_rcb->item, "Resv", 1U, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
            worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_RPTENA_RESERVE_FAILED", diagnostic.message);
            return 0;
        }
    }
    invoke_id = unitlab_native_client_session_reserve_invoke_id(&context->session);
    if (!emit_discovered_rcb_bool_step(&context->session, runtime, context->data_fd, "rptena", selected_rcb->domain, selected_rcb->item, "RptEna", 1U, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_RPTENA_FAILED", diagnostic.message);
        return 0;
    }
    context->session.subscription_model.rpt_enabled = 1;
    unitlab_native_client_session_reset_report_sequence(&context->session);
    context->session.subscription_model.selected_rcb_index = selected_rcb_index;
    context->session.subscription_model.last_rptena_invoke_id = invoke_id;
    snprintf(context->session.subscription_model.rcb_domain, sizeof(context->session.subscription_model.rcb_domain), "%s", selected_rcb->domain);
    snprintf(context->session.subscription_model.rcb_item, sizeof(context->session.subscription_model.rcb_item), "%s", selected_rcb->item);
    if (runtime->intent.wants_gi) {
        uint32_t gi_invoke_id = unitlab_native_client_session_reserve_invoke_id(&context->session);
        if (!emit_discovered_rcb_bool_step(&context->session, runtime, context->data_fd, "gi", selected_rcb->domain, selected_rcb->item, "GI", 1U, gi_invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
            worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_GI_FAILED", diagnostic.message);
            return 0;
        }
        context->session.subscription_model.gi_requested = 1;
        context->session.subscription_model.last_gi_invoke_id = gi_invoke_id;
    }
    emit_subscription_summary(&context->session, runtime->intent.wants_gi ? "gi" : "rptena");
    return 1;
}

int unitlab_native_wire_client_worker_reconnect(
    void* user_data,
    UnitLabNativeSessionRuntime* runtime,
    char* error_code,
    size_t error_code_size,
    char* error_message,
    size_t error_message_size)
{
    UnitLabNativeWireClientWorkerContext* context = (UnitLabNativeWireClientWorkerContext*)user_data;
    UnitLabMmsDiagnostic diagnostic;
    uint8_t scratch[65535U];
    uint8_t read_request[65535U];
    uint8_t report_frame[65535U];
    uint8_t frame[65535U];
    size_t report_length = 0U;

    if (context == NULL || runtime == NULL) {
        worker_copy_error(error_code, error_code_size, error_message, error_message_size, "NATIVE_WIRE_CLIENT_WORKER_INVALID", "Native wire client worker reconnect requires a configured context and runtime.");
        return 0;
    }
    if (context->data_fd >= 0 && context->session.subscription_model.rcb_item[0] != '\0') {
        unitlab_mms_diagnostic_clear(&diagnostic);
        (void)native_wire_client_cleanup_selected_subscription(&context->session, context->data_fd, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic);
    }
    worker_close_transport(context);
    return unitlab_native_wire_client_worker_connect(user_data, runtime, error_code, error_code_size, error_message, error_message_size);
}

static int format_hex_response(const uint8_t* frame, size_t frame_length, char* response, size_t response_length)
{
    static const char hex_digits[] = "0123456789abcdef";
    size_t required_length = 11U + (frame_length * 2U) + 1U;
    size_t offset = 0U;

    if (response == NULL || response_length == 0U) {
        return 0;
    }
    if (response_length < required_length) {
        snprintf(response, response_length, "wire-frame=<omitted length=%zu>", frame_length);
        return 1;
    }

    memcpy(response, "wire-frame=", 11U);
    offset = 11U;
    for (size_t index = 0U; index < frame_length; index++) {
        response[offset++] = hex_digits[(frame[index] >> 4) & 0x0FU];
        response[offset++] = hex_digits[frame[index] & 0x0FU];
    }
    response[offset] = '\0';
    return 1;
}

static int emit_text_response(const char* text)
{
    printf("%s\n", text);
    fflush(stdout);
    return 1;
}

static int emit_wire_frame_response(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    const uint8_t* frame,
    size_t frame_length,
    uint8_t* text_buffer,
    size_t text_buffer_length)
{
    if (!format_hex_response(frame, frame_length, (char*)text_buffer, text_buffer_length) || !emit_text_response((const char*)text_buffer)) {
        return 0;
    }
    if (emit_mms_frame_summary(session, session_runtime, frame, frame_length) && session_runtime != NULL) {
        uint64_t report_timestamp_ms = native_wire_now_ms();
        unitlab_native_session_runtime_apply_last_report_to_signals(session_runtime, session, report_timestamp_ms);
        unitlab_native_session_runtime_mark_report_received(session_runtime, report_timestamp_ms);
    }
    return 1;
}

typedef enum UnitLabNativeWireClientReadStatus {
    UNITLAB_NATIVE_WIRE_CLIENT_READ_OK = 1,
    UNITLAB_NATIVE_WIRE_CLIENT_READ_EOF = 0,
    UNITLAB_NATIVE_WIRE_CLIENT_READ_TIMEOUT = -1,
    UNITLAB_NATIVE_WIRE_CLIENT_READ_MALFORMED = -2,
    UNITLAB_NATIVE_WIRE_CLIENT_READ_OVERSIZED = -3,
    UNITLAB_NATIVE_WIRE_CLIENT_READ_SYSTEM_ERROR = -4
} UnitLabNativeWireClientReadStatus;

static UnitLabNativeWireClientReadStatus read_tpkt_frame_status(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length)
{
    uint8_t header[4U];
    uint16_t total_length;
    size_t offset = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    while (offset < sizeof(header)) {
        ssize_t received = recv(fd, &header[offset], sizeof(header) - offset, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return UNITLAB_NATIVE_WIRE_CLIENT_READ_SYSTEM_ERROR;
        }
        if (received == 0) {
            return UNITLAB_NATIVE_WIRE_CLIENT_READ_EOF;
        }
        offset += (size_t)received;
    }
    if (header[0] != 3U || header[1] != 0U) {
        return UNITLAB_NATIVE_WIRE_CLIENT_READ_MALFORMED;
    }

    total_length = (uint16_t)(((uint16_t)header[2] << 8U) | (uint16_t)header[3]);
    if (total_length < 4U) {
        return UNITLAB_NATIVE_WIRE_CLIENT_READ_MALFORMED;
    }
    if (total_length > frame_length) {
        return UNITLAB_NATIVE_WIRE_CLIENT_READ_OVERSIZED;
    }

    memcpy(frame, header, sizeof(header));
    offset = 4U;
    while (offset < (size_t)total_length) {
        ssize_t received = recv(fd, frame + offset, (size_t)total_length - offset, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return UNITLAB_NATIVE_WIRE_CLIENT_READ_SYSTEM_ERROR;
        }
        if (received == 0) {
            return UNITLAB_NATIVE_WIRE_CLIENT_READ_EOF;
        }
        offset += (size_t)received;
    }
    if (encoded_length != NULL) {
        *encoded_length = (size_t)total_length;
    }
    return UNITLAB_NATIVE_WIRE_CLIENT_READ_OK;
}

static int read_tpkt_frame_status_with_timeout(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length, int timeout_ms)
{
    fd_set read_set;
    struct timeval timeout;
    int ready;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (fd < 0 || frame == NULL || frame_length == 0U || timeout_ms < 0) {
        return UNITLAB_NATIVE_WIRE_CLIENT_READ_SYSTEM_ERROR;
    }

    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    do {
        ready = select(fd + 1, &read_set, NULL, NULL, &timeout);
    } while (ready < 0 && errno == EINTR);

    if (ready < 0) {
        return UNITLAB_NATIVE_WIRE_CLIENT_READ_SYSTEM_ERROR;
    }
    if (ready == 0) {
        return UNITLAB_NATIVE_WIRE_CLIENT_READ_TIMEOUT;
    }
    return read_tpkt_frame_status(fd, frame, frame_length, encoded_length);
}

static int read_tpkt_frame(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length)
{
    return read_tpkt_frame_status(fd, frame, frame_length, encoded_length) == UNITLAB_NATIVE_WIRE_CLIENT_READ_OK;
}

static int read_tpkt_frame_if_available(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length, int timeout_ms)
{
    fd_set read_set;
    struct timeval timeout;
    int ready;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (fd < 0 || frame == NULL || frame_length == 0U || timeout_ms < 0) {
        return -1;
    }

    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    do {
        ready = select(fd + 1, &read_set, NULL, NULL, &timeout);
    } while (ready < 0 && errno == EINTR);

    if (ready < 0) {
        return -1;
    }
    if (ready == 0) {
        return 0;
    }
    return read_tpkt_frame(fd, frame, frame_length, encoded_length) ? 1 : -1;
}

static int validate_association_response_frame(const uint8_t* frame, size_t frame_length, UnitLabIedModelLoadResult* result)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsTransportFrame transport_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsAcseApdu acse_apdu;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed_length = 0U;

    if (frame == NULL || frame_length == 0U) {
        set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_MALFORMED_FRAME", "Native wire client received an empty association response frame.");
        return 0;
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_transport_frame_init(&transport_frame);
    if (unitlab_mms_transport_frame_decode(&transport_frame, frame, frame_length, &consumed_length, &diagnostic)
        && transport_frame.cotp.kind == UNITLAB_MMS_COTP_TPDU_DT
        && !transport_frame.cotp.eot) {
        set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_SEGMENTED_RESPONSE", "Native wire client does not support segmented association responses.");
        return 0;
    }

    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, frame, frame_length, &consumed_length, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_MALFORMED_FRAME", diagnostic.message[0] != '\0' ? diagnostic.message : "Native wire client received a malformed association response frame.");
        return 0;
    }
    if (association_frame.presentation.payload_bytes == NULL || association_frame.presentation.payload_length == 0U) {
        set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_MALFORMED_FRAME", "Native wire client association response is missing presentation payload bytes.");
        return 0;
    }

    unitlab_mms_pdu_init(&pdu);
    if (unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic)) {
        if (pdu.kind == UNITLAB_MMS_PDU_REJECT) {
            set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_REJECTED", "Native wire client association was rejected by an MMS Reject PDU.");
            return 0;
        }
        if (pdu.kind == UNITLAB_MMS_PDU_INITIATE_ERROR) {
            set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_INITIATE_ERROR", "Native wire client association failed with an MMS InitiateError PDU.");
            return 0;
        }
        if (pdu.kind == UNITLAB_MMS_PDU_INITIATE_RESPONSE) {
            return 1;
        }
    }

    unitlab_mms_acse_apdu_init(&acse_apdu);
    if (unitlab_mms_acse_decode(&acse_apdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed_length, &diagnostic)) {
        if (acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_ABRT) {
            set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_ABORTED", "Native wire client association was aborted by an ACSE ABRT APDU.");
            return 0;
        }
        if (acse_apdu.kind == UNITLAB_MMS_ACSE_APDU_AARE) {
            return 1;
        }
    }

    set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_UNEXPECTED_RESPONSE", "Native wire client received an unexpected association response PDU.");
    return 0;
}

static int emit_async_data_frame_if_ready(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    int available = read_tpkt_frame_if_available(data_fd, response, response_length, encoded_response_length, 0);
    if (available < 0) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not receive an async data frame.");
        }
        return -1;
    }
    if (available == 0) {
        return 0;
    }
    printf("native-wire-client: async-report\n");
    if (encoded_response_length == NULL || !emit_wire_frame_response(session, session_runtime, response, *encoded_response_length, text_buffer, text_buffer_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the async data frame.");
        }
        return -1;
    }
    session->subscription_model.async_report_count++;
    emit_subscription_summary(session, "async-report");
    return 1;
}

static int emit_confirmed_response(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    const char* failure_message,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (!send_all(data_fd, request, request_length) || !read_tpkt_frame(data_fd, response, response_length, encoded_response_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", failure_message);
        }
        return 0;
    }
    if (encoded_response_length == NULL || !emit_wire_frame_response(session, NULL, response, *encoded_response_length, text_buffer, text_buffer_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the confirmed response.");
        }
        return 0;
    }
    return 1;
}

static int emit_read_response(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_request_length = 0U;

    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client read target requires domain and item.");
        }
        return 0;
    }
    if (!unitlab_mms_build_read_request_frame(domain_id, item_id, invoke_id, scratch, scratch_length, request, request_length, &encoded_request_length, diagnostic)) {
        return 0;
    }
    session->has_pending_read = 1;
    session->pending_read_invoke_id = invoke_id;
    snprintf(session->pending_read_domain, sizeof(session->pending_read_domain), "%s", domain_id);
    snprintf(session->pending_read_item, sizeof(session->pending_read_item), "%s", item_id);
    if (!emit_confirmed_response(
            session,
            data_fd,
            request,
            encoded_request_length,
            response,
            response_length,
            encoded_response_length,
            text_buffer,
            text_buffer_length,
            "Native wire client could not receive the confirmed-read response.",
            diagnostic)) {
        session->has_pending_read = 0;
        return 0;
    }
    session->has_pending_read = 0;
    return 1;
}

static int emit_write_bool_response(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint8_t boolean_value,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);

static int emit_get_attributes_response(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);

static int emit_get_name_list_response(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* node_id,
    const char* continue_after,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_request_length = 0U;

    if (!unitlab_mms_build_get_name_list_request_frame_ex(
            object_class,
            object_scope,
            domain_id,
            node_id,
            continue_after,
            invoke_id,
            scratch,
            scratch_length,
            request,
            request_length,
            &encoded_request_length,
            diagnostic)) {
        return 0;
    }
    return emit_confirmed_response(
        session,
        data_fd,
        request,
        encoded_request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        "Native wire client could not receive the GetNameList response.",
        diagnostic);
}

static int emit_discover_get_name_list_step(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* label,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* node_id,
    const char* continue_after,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    printf(
        "native-wire-client: discover-step=%s invoke=%u class=%u scope=%u domain=%s node=%s continue-after=%s\n",
        label != NULL ? label : "get-name-list",
        (unsigned)invoke_id,
        (unsigned)object_class,
        (unsigned)object_scope,
        domain_id != NULL ? domain_id : "<none>",
        node_id != NULL ? node_id : "<none>",
        continue_after != NULL ? continue_after : "<none>");
    fflush(stdout);
    return emit_get_name_list_response(
        session,
        data_fd,
        object_class,
        object_scope,
        domain_id,
        node_id,
        continue_after,
        invoke_id,
        scratch,
        scratch_length,
        request,
        request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        diagnostic);
}


static int emit_discover_read_step(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    printf(
        "native-wire-client: discover-step=%s invoke=%u domain=%s item=%s\n",
        label != NULL ? label : "read",
        (unsigned)invoke_id,
        domain_id != NULL ? domain_id : "<none>",
        item_id != NULL ? item_id : "<none>");
    fflush(stdout);
    return emit_read_response(
        session,
        data_fd,
        domain_id,
        item_id,
        invoke_id,
        scratch,
        scratch_length,
        request,
        request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        diagnostic);
}

static int emit_discovered_rcb_bool_step(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* rcb_item,
    const char* field_name,
    uint8_t boolean_value,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char item_id[384U];
    int written;

    if (domain_id == NULL || domain_id[0] == '\0' || rcb_item == NULL || rcb_item[0] == '\0' || field_name == NULL || field_name[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client discovered RCB write requires domain, RCB item, and field.");
        }
        return 0;
    }
    written = snprintf(item_id, sizeof(item_id), "%s$%s", rcb_item, field_name);
    if (written < 0 || (size_t)written >= sizeof(item_id)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client discovered RCB field item is too long.");
        }
        return 0;
    }
    printf(
        "native-wire-client: %s invoke=%u domain=%s item=%s value=%s\n",
        label != NULL ? label : "rcb-write",
        (unsigned)invoke_id,
        domain_id,
        item_id,
        boolean_value != 0U ? "true" : "false");
    fflush(stdout);
    return emit_write_bool_response(
        session,
        session_runtime,
        data_fd,
        domain_id,
        item_id,
        boolean_value,
        invoke_id,
        scratch,
        scratch_length,
        request,
        request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        diagnostic);
}

static int emit_write_element_response(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);

static int emit_get_attributes_response(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic);

static int emit_immediate_post_write_frame_if_available(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    int had_report;
    int extra_frame = read_tpkt_frame_if_available(data_fd, response, response_length, encoded_response_length, 100);
    if (extra_frame < 0) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not receive an immediate post-write frame.");
        }
        return 0;
    }
    if (extra_frame == 0) {
        return 1;
    }
    had_report = session != NULL && session->subscription_model.last_report_received;
    if (encoded_response_length == NULL || !emit_wire_frame_response(session, session_runtime, response, *encoded_response_length, text_buffer, text_buffer_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the immediate post-write frame.");
        }
        return 0;
    }
    if (session != NULL && !had_report && session->subscription_model.last_report_received) {
        printf("native-wire-client: async-report\n");
        session->subscription_model.async_report_count++;
        emit_subscription_summary(session, "async-report");
    }
    return 1;
}

static size_t encode_uint32_be_minimal(uint32_t value, uint8_t* output, size_t output_size)
{
    size_t length = 0U;

    if (output == NULL || output_size == 0U) {
        return 0U;
    }
    if (value == 0U) {
        output[0] = 0U;
        return 1U;
    }
    while (value != 0U && length < output_size) {
        output[output_size - 1U - length] = (uint8_t)(value & 0xFFU);
        value >>= 8U;
        length++;
    }
    if (length == 0U || length > output_size) {
        return 0U;
    }
    memmove(output, &output[output_size - length], length);
    return length;
}

static int native_wire_client_preflight_selected_rcb(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* rcb_item,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (domain_id == NULL || domain_id[0] == '\0' || rcb_item == NULL || rcb_item[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client RCB preflight requires domain and item.");
        }
        return 0;
    }
    printf(
        "native-wire-client: %s invoke=%u domain=%s item=%s\n",
        label != NULL ? label : "rcb-preflight",
        (unsigned)invoke_id,
        domain_id,
        rcb_item);
    fflush(stdout);
    return emit_get_attributes_response(
        session,
        data_fd,
        domain_id,
        rcb_item,
        invoke_id,
        0,
        scratch,
        scratch_length,
        request,
        request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        diagnostic);
}

static int emit_discovered_rcb_unsigned_step(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* rcb_item,
    const char* field_name,
    uint32_t value,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char item_id[384U];
    uint8_t encoded_value[4U];
    size_t encoded_value_length = 0U;
    int written;

    if (domain_id == NULL || domain_id[0] == '\0' || rcb_item == NULL || rcb_item[0] == '\0' || field_name == NULL || field_name[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client discovered RCB write requires domain, RCB item, and field.");
        }
        return 0;
    }
    written = snprintf(item_id, sizeof(item_id), "%s$%s", rcb_item, field_name);
    if (written < 0 || (size_t)written >= sizeof(item_id)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client discovered RCB field item is too long.");
        }
        return 0;
    }
    encoded_value_length = encode_uint32_be_minimal(value, encoded_value, sizeof(encoded_value));
    if (encoded_value_length == 0U) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not encode RCB unsigned value.");
        }
        return 0;
    }
    printf(
        "native-wire-client: %s invoke=%u domain=%s item=%s value=%u\n",
        label != NULL ? label : "rcb-write",
        (unsigned)invoke_id,
        domain_id,
        item_id,
        (unsigned)value);
    fflush(stdout);
    return emit_write_element_response(
        session,
        NULL,
        data_fd,
        domain_id,
        item_id,
        5U,
        encoded_value,
        encoded_value_length,
        invoke_id,
        scratch,
        scratch_length,
        request,
        request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        diagnostic);
}

static int native_wire_client_subscription_is_buffered_rcb(const UnitLabNativeClientSessionState* session)
{
    return session != NULL
        && session->subscription_model.rcb_item[0] != '\0'
        && strstr(session->subscription_model.rcb_item, "$BR$") != NULL;
}

static int native_wire_client_subscription_is_unbuffered_rcb(const UnitLabNativeClientSessionState* session)
{
    return session != NULL
        && session->subscription_model.rcb_item[0] != '\0'
        && strstr(session->subscription_model.rcb_item, "$RP$") != NULL;
}

static int native_wire_client_cleanup_selected_subscription(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint32_t invoke_id;
    const char* domain_id;
    const char* rcb_item;

    if (session == NULL) {
        return 1;
    }
    domain_id = session->subscription_model.rcb_domain;
    rcb_item = session->subscription_model.rcb_item;
    if (domain_id[0] == '\0' || rcb_item[0] == '\0') {
        return 1;
    }
    if (session->subscription_model.rpt_enabled) {
        invoke_id = unitlab_native_client_session_reserve_invoke_id(session);
        if (!emit_discovered_rcb_bool_step(session, NULL, data_fd, "cleanup-rptena", domain_id, rcb_item, "RptEna", 0U, invoke_id, scratch, scratch_length, request, request_length, response, response_length, encoded_response_length, text_buffer, text_buffer_length, diagnostic)) {
            return 0;
        }
    }
    if (native_wire_client_subscription_is_buffered_rcb(session)) {
        invoke_id = unitlab_native_client_session_reserve_invoke_id(session);
        if (!emit_discovered_rcb_unsigned_step(session, data_fd, "cleanup-resvtms", domain_id, rcb_item, "ResvTms", 0U, invoke_id, scratch, scratch_length, request, request_length, response, response_length, encoded_response_length, text_buffer, text_buffer_length, diagnostic)) {
            return 0;
        }
    } else if (native_wire_client_subscription_is_unbuffered_rcb(session)) {
        invoke_id = unitlab_native_client_session_reserve_invoke_id(session);
        if (!emit_discovered_rcb_bool_step(session, NULL, data_fd, "cleanup-resv", domain_id, rcb_item, "Resv", 0U, invoke_id, scratch, scratch_length, request, request_length, response, response_length, encoded_response_length, text_buffer, text_buffer_length, diagnostic)) {
            return 0;
        }
    }
    session->subscription_model.rpt_enabled = 0;
    session->subscription_model.gi_requested = 0;
    session->subscription_model.selected_rcb_index = 0U;
    session->subscription_model.last_rptena_invoke_id = 0U;
    session->subscription_model.last_gi_invoke_id = 0U;
    session->subscription_model.rcb_domain[0] = '\0';
    session->subscription_model.rcb_item[0] = '\0';
    unitlab_native_client_session_reset_report_sequence(session);
    return 1;
}

static int emit_discover_attributes_step(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    printf(
        "native-wire-client: discover-step=%s invoke=%u domain=%s item=%s\n",
        label != NULL ? label : "attributes",
        (unsigned)invoke_id,
        domain_id != NULL ? domain_id : "<none>",
        item_id != NULL ? item_id : "<none>");
    fflush(stdout);
    return emit_get_attributes_response(
        session,
        data_fd,
        domain_id,
        item_id,
        invoke_id,
        named_variable_list,
        scratch,
        scratch_length,
        request,
        request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        diagnostic);
}

static int discovery_get_name_list_step_adapter(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
    const char* node_id,
    const char* continue_after,
    uint32_t invoke_id)
{
    return emit_discover_get_name_list_step(
        session,
        io->data_fd,
        label,
        object_class,
        object_scope,
        domain_id,
        node_id,
        continue_after,
        invoke_id,
        io->scratch,
        io->scratch_length,
        io->request,
        io->request_length,
        io->response,
        io->response_length,
        io->encoded_response_length,
        io->text_buffer,
        io->text_buffer_length,
        io->diagnostic);
}

static int discovery_read_step_adapter(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id)
{
    return emit_discover_read_step(
        session,
        io->data_fd,
        label,
        domain_id,
        item_id,
        invoke_id,
        io->scratch,
        io->scratch_length,
        io->request,
        io->request_length,
        io->response,
        io->response_length,
        io->encoded_response_length,
        io->text_buffer,
        io->text_buffer_length,
        io->diagnostic);
}

static int discovery_attributes_step_adapter(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* label,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list)
{
    return emit_discover_attributes_step(
        session,
        io->data_fd,
        label,
        domain_id,
        item_id,
        invoke_id,
        named_variable_list,
        io->scratch,
        io->scratch_length,
        io->request,
        io->request_length,
        io->response,
        io->response_length,
        io->encoded_response_length,
        io->text_buffer,
        io->text_buffer_length,
        io->diagnostic);
}

static int emit_get_attributes_response(
    UnitLabNativeClientSessionState* session,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t invoke_id,
    int named_variable_list,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    size_t encoded_request_length = 0U;

    if (item_id == NULL || item_id[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client attribute request requires an item.");
        }
        return 0;
    }
    if (named_variable_list) {
        if (!unitlab_mms_build_get_named_variable_list_attributes_request_frame(
                domain_id,
                item_id,
                invoke_id,
                scratch,
                scratch_length,
                request,
                request_length,
                &encoded_request_length,
                diagnostic)) {
            return 0;
        }
    } else {
        if (!unitlab_mms_build_get_variable_access_attributes_request_frame(
                domain_id,
                item_id,
                invoke_id,
                scratch,
                scratch_length,
                request,
                request_length,
                &encoded_request_length,
                diagnostic)) {
            return 0;
        }
    }
    return emit_confirmed_response(
        session,
        data_fd,
        request,
        encoded_request_length,
        response,
        response_length,
        encoded_response_length,
        text_buffer,
        text_buffer_length,
        named_variable_list
            ? "Native wire client could not receive the GetNamedVariableListAttributes response."
            : "Native wire client could not receive the GetVariableAccessAttributes response.",
        diagnostic);
}

static int emit_write_bool_response(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint8_t boolean_value,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement data_element;
    size_t encoded_request_length = 0U;

    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0') {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client write target requires domain and item.");
        }
        return 0;
    }

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = 3U;
    data_element.value_bytes = &boolean_value;
    data_element.value_length = 1U;

    if (!unitlab_mms_build_write_request_frame(
            domain_id,
            item_id,
            &data_element,
            invoke_id,
            scratch,
            scratch_length,
            request,
            request_length,
            &encoded_request_length,
            diagnostic)) {
        return 0;
    }
    if (!emit_confirmed_response(
            session,
            data_fd,
            request,
            encoded_request_length,
            response,
            response_length,
            encoded_response_length,
            text_buffer,
            text_buffer_length,
            "Native wire client could not receive the confirmed-write response.",
            diagnostic)) {
        return 0;
    }
    if (!validate_confirmed_write_response(response, *encoded_response_length, diagnostic)) {
        return 0;
    }

    if (!emit_immediate_post_write_frame_if_available(
            session,
            session_runtime,
            data_fd,
            response,
            response_length,
            encoded_response_length,
            text_buffer,
            text_buffer_length,
            diagnostic)) {
        return 0;
    }
    return 1;
}

static int emit_write_element_response(
    UnitLabNativeClientSessionState* session,
    UnitLabNativeSessionRuntime* session_runtime,
    int data_fd,
    const char* domain_id,
    const char* item_id,
    uint32_t tag_number,
    const uint8_t* value_bytes,
    size_t value_length,
    uint32_t invoke_id,
    uint8_t* scratch,
    size_t scratch_length,
    uint8_t* request,
    size_t request_length,
    uint8_t* response,
    size_t response_length,
    size_t* encoded_response_length,
    uint8_t* text_buffer,
    size_t text_buffer_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement data_element;
    size_t encoded_request_length = 0U;

    if (domain_id == NULL || domain_id[0] == '\0' || item_id == NULL || item_id[0] == '\0' || value_bytes == NULL) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client typed write requires domain, item, and value.");
        }
        return 0;
    }

    unitlab_mms_ber_element_init(&data_element);
    data_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    data_element.tag.constructed = 0;
    data_element.tag.tag_number = tag_number;
    data_element.value_bytes = value_bytes;
    data_element.value_length = value_length;

    if (!unitlab_mms_build_write_request_frame(
            domain_id,
            item_id,
            &data_element,
            invoke_id,
            scratch,
            scratch_length,
            request,
            request_length,
            &encoded_request_length,
            diagnostic)) {
        return 0;
    }
    if (!emit_confirmed_response(
            session,
            data_fd,
            request,
            encoded_request_length,
            response,
            response_length,
            encoded_response_length,
            text_buffer,
            text_buffer_length,
            "Native wire client could not receive the confirmed-write response.",
            diagnostic)) {
        return 0;
    }
    if (!validate_confirmed_write_response(response, *encoded_response_length, diagnostic)) {
        return 0;
    }

    if (!emit_immediate_post_write_frame_if_available(
            session,
            session_runtime,
            data_fd,
            response,
            response_length,
            encoded_response_length,
            text_buffer,
            text_buffer_length,
            diagnostic)) {
        return 0;
    }
    return 1;
}

static int native_wire_client_wait_for_reports(
    UnitLabNativeWireClientWorkerContext* context,
    UnitLabNativeSessionRuntime* runtime,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    uint8_t report_frame[65535U];
    uint8_t text_buffer[65535U];
    size_t report_length = 0U;
    UnitLabMmsDiagnostic diagnostic;

    if (context == NULL || runtime == NULL) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    while (stop_requested == NULL || !stop_requested(stop_context)) {
        fd_set read_set;
        struct timeval timeout;
        int ready;

        if (context->data_fd < 0) {
            break;
        }
        FD_ZERO(&read_set);
        FD_SET(context->data_fd, &read_set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 250000;
        do {
            ready = select(context->data_fd + 1, &read_set, NULL, NULL, &timeout);
        } while (ready < 0 && errno == EINTR);
        if (ready < 0) {
            set_result(result, "NATIVE_WIRE_CLIENT_SELECT_FAILED", "Native wire client command/report wait failed.");
            return 0;
        }
        if (ready == 0) {
            continue;
        }
        if (emit_async_data_frame_if_ready(
                &context->session,
                runtime,
                context->data_fd,
                report_frame,
                sizeof(report_frame),
                &report_length,
                text_buffer,
                sizeof(text_buffer),
                &diagnostic) < 0) {
            set_result(result, "NATIVE_WIRE_CLIENT_ASYNC_REPORT_FAILED", diagnostic.message);
            return 0;
        }
    }
    return 1;
}

int unitlab_run_native_wire_client_with_options(
    const UnitLabIedServerConfig* config,
    const UnitLabNativeWireClientOptions* options,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    UnitLabNativeSessionManager session_manager;
    UnitLabNativeWireClientWorkerContext worker_context;
    UnitLabNativeSessionWorker worker;
    UnitLabNativeSessionWorkerHandlers handlers;
    UnitLabNativeSessionRuntime* session_runtime = NULL;
    char endpoint_id[192U];
    char device_key[160U];
    const char* initial_read_domain = NULL;
    const char* initial_read_item = NULL;
    uint32_t initial_read_invoke_id = 3U;
    uint8_t read_request[65535U];
    uint8_t scratch[65535U];
    uint8_t report_frame[65535U];
    uint8_t text_buffer[65535U];
    size_t report_length = 0U;
    UnitLabMmsDiagnostic diagnostic;
    int worker_started = 0;
    int success = 0;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (config == NULL || result == NULL) {
        set_result(result, "NATIVE_WIRE_CLIENT_INVALID_ARGUMENT", "Native wire client requires config and result.");
        return 0;
    }
    if (config->bind_address == NULL || config->bind_address[0] == '\0') {
        set_result(result, "NATIVE_WIRE_CLIENT_HOST_REQUIRED", "Native wire client requires a target host.");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535 || config->control_port < 0 || config->control_port > 65535) {
        set_result(result, "NATIVE_WIRE_CLIENT_PORT_INVALID", "Native wire client target ports are invalid.");
        return 0;
    }

    unitlab_native_session_manager_init(&session_manager);
    unitlab_native_wire_client_worker_context_init(&worker_context, config);
    unitlab_native_wire_client_worker_default_handlers(&handlers);
    native_wire_format_endpoint_id(config, endpoint_id, sizeof(endpoint_id));
    native_wire_copy_text(device_key, sizeof(device_key), endpoint_id[0] != '\0' ? endpoint_id : "native-wire-client");
    session_runtime = unitlab_native_session_manager_get_or_create(&session_manager, "native-wire-client", endpoint_id, device_key);
    if (session_runtime == NULL) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not allocate session runtime.");
        goto cleanup;
    }

    unitlab_native_session_runtime_set_desired_state(session_runtime, 1, 0, 0, 0);
    if (options != NULL && options->initial_read_enabled) {
        if (options->initial_read_domain != NULL && options->initial_read_domain[0] != '\0') {
            initial_read_domain = options->initial_read_domain;
        }
        if (options->initial_read_item != NULL && options->initial_read_item[0] != '\0') {
            initial_read_item = options->initial_read_item;
        }
        if (options->initial_read_invoke_id != 0U) {
            initial_read_invoke_id = options->initial_read_invoke_id;
        }
    }
    if (initial_read_domain != NULL && initial_read_item != NULL) {
        unitlab_native_client_session_observe_invoke_id(&worker_context.session, initial_read_invoke_id);
    }

    unitlab_native_session_worker_init(&worker, &session_manager, session_runtime, &handlers, &worker_context);
    if (!unitlab_native_session_worker_start(&worker)) {
        set_result(result, "NATIVE_WIRE_CLIENT_WORKER_START_FAILED", "Native wire client could not start the session worker.");
        goto cleanup;
    }
    worker_started = 1;
    if (!unitlab_native_session_worker_run_until_idle(&worker, 4U) || !session_runtime->live.associated) {
        set_result(
            result,
            session_runtime->live.last_error_code[0] != '\0' ? session_runtime->live.last_error_code : "NATIVE_WIRE_CLIENT_CONNECT_FAILED",
            session_runtime->live.last_error_message[0] != '\0' ? session_runtime->live.last_error_message : "Native wire client could not establish an associated session.");
        goto cleanup;
    }

    if (initial_read_domain != NULL && initial_read_item != NULL) {
        unitlab_mms_diagnostic_clear(&diagnostic);
        if (!emit_state_response(UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED)) {
            set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its read-requested state.");
            goto cleanup;
        }
        if (!emit_read_response(
                &worker_context.session,
                worker_context.data_fd,
                initial_read_domain,
                initial_read_item,
                initial_read_invoke_id,
                scratch,
                sizeof(scratch),
                read_request,
                sizeof(read_request),
                report_frame,
                sizeof(report_frame),
                &report_length,
                text_buffer,
                sizeof(text_buffer),
                &diagnostic)) {
            set_result(result, "NATIVE_WIRE_CLIENT_READ_FAILED", diagnostic.message);
            goto cleanup;
        }
    }

    if (!emit_state_response(UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state.");
        goto cleanup;
    }
    if (!emit_text_response("native-wire-client: ready")) {
        set_result(result, "NATIVE_WIRE_CLIENT_READY_FAILED", "Native wire client could not emit its ready banner.");
        goto cleanup;
    }

    if (!native_wire_client_wait_for_reports(&worker_context, session_runtime, result, stop_requested, stop_context)) {
        goto cleanup;
    }

    set_result(result, "NATIVE_WIRE_CLIENT_STOPPED", "Native wire client stopped.");
    success = 1;

cleanup:
    if (worker_started) {
        unitlab_native_session_worker_stop(&worker);
    }
    unitlab_native_wire_client_worker_context_reset(&worker_context);
    unitlab_native_session_manager_reset(&session_manager);
    return success;
}

int unitlab_run_native_wire_client(
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    return unitlab_run_native_wire_client_with_options(config, NULL, result, stop_requested, stop_context);
}
