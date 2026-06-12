#define _POSIX_C_SOURCE 200112L
#include "native_wire_client.h"
#include "native_wire_client_ber_helpers.h"
#include "native_wire_client_session.h"
#include "native_wire_client_discovery.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "wire/orchestration/unitlab_mms_association_frame.h"
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
        "native-wire-client: subscription-summary phase=%s rcb=%s/%s rcb-index=%zu rptEna=%s rptEna-invoke=%u giRequested=%s gi-invoke=%u lastReportReceived=%s asyncReports=%zu lastReportValues=%zu lastReportDataRefs=%zu lastReportMatchedDataRefs=%zu lastReportReasons=%zu\n",
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
        session->discovered_model.last_report_value_count,
        session->discovered_model.last_report_data_ref_count,
        session->discovered_model.last_report_matched_data_ref_count,
        session->discovered_model.last_report_reason_count);
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
        "native-wire-client: model-summary phase=%s domain=%s logical-devices=%zu logical-nodes=%zu data-names=%zu typed-data-names=%zu data-components=%zu typed-data-components=%zu leaf-refs=%zu datasets=%zu dataset-members=%zu brcbs=%zu last-report-entries=%zu last-report-dataRefs=%zu last-report-values=%zu last-report-reasons=%zu last-report-matched-dataRefs=%zu last-report-rptId=%s last-report-datSet=%s\n",
        phase != NULL ? phase : "snapshot",
        session->discovered_model.domain[0] != '\0' ? session->discovered_model.domain : "<none>",
        session->discovered_model.logical_device_count,
        session->discovered_model.logical_node_count,
        session->discovered_model.data_name_count,
        typed_data_name_count,
        session->discovered_model.data_component_count,
        typed_data_component_count,
        session->discovered_model.leaf_ref_count,
        session->discovered_model.data_set_count,
        session->discovered_model.data_set_member_count,
        session->discovered_model.brcb_count,
        session->last_report_entry_count,
        session->discovered_model.last_report_data_ref_count,
        session->discovered_model.last_report_value_count,
        session->discovered_model.last_report_reason_count,
        session->discovered_model.last_report_matched_data_ref_count,
        session->discovered_model.last_report_rpt_id[0] != '\0' ? session->discovered_model.last_report_rpt_id : "<none>",
        session->discovered_model.last_report_data_set[0] != '\0' ? session->discovered_model.last_report_data_set : "<none>");
    fflush(stdout);
}

static int send_all(int fd, const uint8_t* buffer, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t written = send(fd, buffer + offset, length - offset, 0);
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

static int read_exact(int fd, uint8_t* buffer, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t received = recv(fd, buffer + offset, length - offset, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (received == 0) {
            return 0;
        }
        offset += (size_t)received;
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
    if (buffer == NULL || buffer_size == 0U) {
        return;
    }
    buffer[0] = '\0';
    if (value == NULL || value->value_bytes == NULL || value->value_length == 0U) {
        snprintf(buffer, buffer_size, "%s", "<empty>");
        return;
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

static void copy_report_reason_metadata(const UnitLabMmsBerElement* reason, UnitLabNativeLastReportEntry* entry)
{
    if (entry == NULL) {
        return;
    }
    entry->raw_reason_tag_class = 0U;
    entry->raw_reason_tag_number = 0U;
    entry->raw_reason_length = 0U;
    entry->reason_code = 0U;
    if (reason == NULL) {
        return;
    }
    entry->raw_reason_tag_class = (uint8_t)reason->tag.tag_class;
    entry->raw_reason_tag_number = (uint8_t)reason->tag.tag_number;
    entry->raw_reason_length = reason->value_length;
    if (reason->value_bytes != NULL && reason->value_length > 0U) {
        entry->reason_code = decode_unsigned_bytes(reason->value_bytes, reason->value_length);
    }
}

static void copy_report_typed_value(const UnitLabMmsBerElement* value, UnitLabNativeLastReportEntry* entry)
{
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
    if (value == NULL) {
        return;
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
    } else if (value->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && value->tag.tag_number == 9U) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_OCTETS;
    } else if (unitlab_native_client_bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
        entry->value_kind = UNITLAB_NATIVE_REPORT_VALUE_STRING;
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

static void emit_information_report_summary(UnitLabNativeClientSessionState* session, const UnitLabMmsPdu* pdu)
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
    const uint8_t* opt_flds = NULL;
    size_t opt_flds_length = 0U;
    int has_data_reference = 0;
    int has_reason = 0;
    int report_data_set_discovered = 0;
    const uint8_t* inclusion_bytes = NULL;
    size_t inclusion_length = 0U;

    if (pdu == NULL || pdu->service_bytes == NULL || pdu->service_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_native_client_session_reset_last_report(session);
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
    (void)copy_printable_value(&value, session->discovered_model.last_report_rpt_id, sizeof(session->discovered_model.last_report_rpt_id));
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
        printf("mms-summary: report.SqNum=");
        print_report_value_summary(&value);
        printf("\n");
    }
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
        }
        printf("mms-summary: report.value[%zu]=", value_count);
        print_report_value_summary(&next);
        if (value_count < session->last_report_entry_count) {
            printf(" ref=%s kind=%s", session->last_report_entries[value_count].display_reference, report_value_kind_label(session->last_report_entries[value_count].value_kind));
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
                printf(" ref=%s reason-code=0x%04x", session->last_report_entries[reason_count].display_reference, (unsigned)session->last_report_entries[reason_count].reason_code);
            }
            printf("\n");
            reason_count++;
        }
    }
    session->discovered_model.last_report_data_ref_count = data_ref_count;
    session->discovered_model.last_report_value_count = value_count;
    session->discovered_model.last_report_reason_count = reason_count;
    session->discovered_model.last_report_matched_data_ref_count = matched_data_ref_count;
    session->subscription_model.last_report_received = 1;
    printf("mms-summary: report.dataRef-count=%zu value-count=%zu reason-count=%zu mapped-entry-count=%zu\n", data_ref_count, value_count, reason_count, session->last_report_entry_count);
    emit_discovered_model_summary(session, "report");
    emit_subscription_summary(session, "report");
    fflush(stdout);
}

static void emit_mms_frame_summary(UnitLabNativeClientSessionState* session, const uint8_t* frame, size_t frame_length)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed = 0U;

    if (session == NULL || frame == NULL || frame_length == 0U) {
        return;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, frame, frame_length, &consumed, &diagnostic)) {
        return;
    }
    if (association_frame.presentation.payload_bytes == NULL || association_frame.presentation.payload_length == 0U) {
        return;
    }
    unitlab_mms_pdu_init(&pdu);
    if (!unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed, &diagnostic)) {
        return;
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
        emit_information_report_summary(session, &pdu);
    }
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
        return 0;
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

static int emit_wire_frame_response(UnitLabNativeClientSessionState* session, const uint8_t* frame, size_t frame_length, uint8_t* text_buffer, size_t text_buffer_length)
{
    if (!format_hex_response(frame, frame_length, (char*)text_buffer, text_buffer_length) || !emit_text_response((const char*)text_buffer)) {
        return 0;
    }
    emit_mms_frame_summary(session, frame, frame_length);
    return 1;
}

static int read_tpkt_frame(int fd, uint8_t* frame, size_t frame_length, size_t* encoded_length)
{
    uint8_t header[4U];
    uint16_t total_length;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (!read_exact(fd, header, sizeof(header))) {
        return 0;
    }
    if (header[0] != 3U || header[1] != 0U) {
        return 0;
    }

    total_length = (uint16_t)(((uint16_t)header[2] << 8U) | (uint16_t)header[3]);
    if (total_length < 4U || total_length > frame_length) {
        return 0;
    }

    memcpy(frame, header, sizeof(header));
    if (!read_exact(fd, &frame[4], (size_t)total_length - 4U)) {
        return 0;
    }
    if (encoded_length != NULL) {
        *encoded_length = (size_t)total_length;
    }
    return 1;
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

static int send_report_control_command(int control_fd, const char* command)
{
    size_t length = strlen(command);
    return send_all(control_fd, (const uint8_t*)command, length) && send_all(control_fd, (const uint8_t*)"\n", 1U);
}

static int emit_async_data_frame_if_ready(
    UnitLabNativeClientSessionState* session,
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
    if (encoded_response_length == NULL || !emit_wire_frame_response(session, response, *encoded_response_length, text_buffer, text_buffer_length)) {
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
    if (encoded_response_length == NULL || !emit_wire_frame_response(session, response, *encoded_response_length, text_buffer, text_buffer_length)) {
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
        "Native wire client could not receive the confirmed-read response.",
        diagnostic);
}

static int emit_write_bool_response(
    UnitLabNativeClientSessionState* session,
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

    {
        int extra_frame = read_tpkt_frame_if_available(data_fd, response, response_length, encoded_response_length, 100);
        if (extra_frame < 0) {
            if (diagnostic != NULL) {
                diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not receive an immediate post-write frame.");
            }
            return 0;
        }
        if (extra_frame > 0) {
            if (encoded_response_length == NULL || !emit_wire_frame_response(session, response, *encoded_response_length, text_buffer, text_buffer_length)) {
                if (diagnostic != NULL) {
                    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
                    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the immediate post-write frame.");
                }
                return 0;
            }
        }
    }
    return 1;
}

static int emit_write_element_response(
    UnitLabNativeClientSessionState* session,
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

    {
        int extra_frame = read_tpkt_frame_if_available(data_fd, response, response_length, encoded_response_length, 100);
        if (extra_frame < 0) {
            if (diagnostic != NULL) {
                diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR;
                snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not receive an immediate post-write frame.");
            }
            return 0;
        }
        if (extra_frame > 0) {
            if (encoded_response_length == NULL || !emit_wire_frame_response(session, response, *encoded_response_length, text_buffer, text_buffer_length)) {
                if (diagnostic != NULL) {
                    diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
                    snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the immediate post-write frame.");
                }
                return 0;
            }
        }
    }
    return 1;
}

static int parse_uint32_token(const char* text, uint32_t* value)
{
    char* end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return 0;
    }
    parsed = strtoul(text, &end, 10);
    if (text == end || end == NULL || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int parse_invoke_id_token(const char* text, uint32_t* value)
{
    return parse_uint32_token(text, value) && *value != 0U;
}

static int encode_uint32_value(uint32_t value, uint8_t* buffer, size_t buffer_length, size_t* encoded_length)
{
    uint8_t temp[4U];
    size_t offset = 0U;

    if (buffer == NULL || encoded_length == NULL || buffer_length == 0U) {
        return 0;
    }
    temp[0] = (uint8_t)((value >> 24U) & 0xFFU);
    temp[1] = (uint8_t)((value >> 16U) & 0xFFU);
    temp[2] = (uint8_t)((value >> 8U) & 0xFFU);
    temp[3] = (uint8_t)(value & 0xFFU);
    while (offset < sizeof(temp) - 1U && temp[offset] == 0U) {
        offset++;
    }
    if (sizeof(temp) - offset > buffer_length) {
        return 0;
    }
    memcpy(buffer, &temp[offset], sizeof(temp) - offset);
    *encoded_length = sizeof(temp) - offset;
    return 1;
}

static int decode_hex_nibble(char value, uint8_t* nibble)
{
    if (nibble == NULL) {
        return 0;
    }
    if (value >= '0' && value <= '9') {
        *nibble = (uint8_t)(value - '0');
        return 1;
    }
    if (value >= 'a' && value <= 'f') {
        *nibble = (uint8_t)(value - 'a' + 10);
        return 1;
    }
    if (value >= 'A' && value <= 'F') {
        *nibble = (uint8_t)(value - 'A' + 10);
        return 1;
    }
    return 0;
}

static int decode_hex_value(const char* text, uint8_t* buffer, size_t buffer_length, size_t* decoded_length)
{
    size_t text_length;

    if (text == NULL || buffer == NULL || decoded_length == NULL) {
        return 0;
    }
    text_length = strlen(text);
    if (text_length == 0U || (text_length % 2U) != 0U || text_length / 2U > buffer_length) {
        return 0;
    }
    for (size_t index = 0U; index < text_length / 2U; index++) {
        uint8_t high = 0U;
        uint8_t low = 0U;
        if (!decode_hex_nibble(text[index * 2U], &high) || !decode_hex_nibble(text[index * 2U + 1U], &low)) {
            return 0;
        }
        buffer[index] = (uint8_t)((high << 4U) | low);
    }
    *decoded_length = text_length / 2U;
    return 1;
}

static int parse_bool_token(const char* text, uint8_t* value)
{
    if (text == NULL || value == NULL) {
        return 0;
    }
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) {
        *value = 0x01U;
        return 1;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) {
        *value = 0x00U;
        return 1;
    }
    return 0;
}

int unitlab_run_native_wire_client_with_options(
    const UnitLabIedServerConfig* config,
    const UnitLabNativeWireClientOptions* options,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    int data_fd = -1;
    int control_fd = -1;
    uint8_t frame[2048U];
    uint8_t scratch[2048U];
    size_t encoded_length = 0U;
    uint8_t association_request[2048U];
    size_t association_length = 0U;
    uint8_t read_request[2048U];
    uint8_t report_frame[2048U];
    size_t report_length = 0U;
    const UnitLabNativeDiscoveredRcb* selected_rcb = NULL;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabNativeClientSessionState session = {0};
    const char* initial_read_domain = "XCBR1";
    const char* initial_read_item = "ST$Pos$stVal";
    uint32_t initial_read_invoke_id = 3U;
    uint32_t next_invoke_id = 4U;
    UnitLabNativeWireClientState state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_INIT;

    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (config == NULL || result == NULL) {
        set_result(result, "NATIVE_WIRE_CLIENT_INVALID_ARGUMENT", "Native wire client requires config and result.");
        return 0;
    }
    unitlab_native_client_session_reset(&session);
    if (config->bind_address == NULL || config->bind_address[0] == '\0') {
        set_result(result, "NATIVE_WIRE_CLIENT_HOST_REQUIRED", "Native wire client requires a target host.");
        return 0;
    }
    if (config->port <= 0 || config->port > 65535 || config->control_port <= 0 || config->control_port > 65535) {
        set_result(result, "NATIVE_WIRE_CLIENT_PORT_INVALID", "Native wire client target ports are invalid.");
        return 0;
    }
    if (options != NULL) {
        if (options->initial_read_domain != NULL && options->initial_read_domain[0] != '\0') {
            initial_read_domain = options->initial_read_domain;
        }
        if (options->initial_read_item != NULL && options->initial_read_item[0] != '\0') {
            initial_read_item = options->initial_read_item;
        }
        if (options->initial_read_invoke_id != 0U) {
            initial_read_invoke_id = options->initial_read_invoke_id;
            next_invoke_id = initial_read_invoke_id + 1U;
        }
    }

    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its initial state.");
        return 0;
    }

    unitlab_mms_diagnostic_clear(&diagnostic);
    data_fd = connect_socket(config->bind_address, config->port);
    if (data_fd < 0) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_CONNECT_FAILED", "Native wire client could not connect to the data endpoint.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_DATA_CONNECTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its data-connected state.");
        goto fail;
    }
    control_fd = connect_socket(config->bind_address, config->control_port);
    if (control_fd < 0) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_CONTROL_CONNECT_FAILED", "Native wire client could not connect to the control endpoint.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_CONTROL_CONNECTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its control-connected state.");
        goto fail;
    }

    if (!unitlab_mms_build_cotp_connect_request_frame(frame, sizeof(frame), &encoded_length, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        goto fail;
    }
    if (!send_all(data_fd, frame, encoded_length) || !read_tpkt_frame(data_fd, frame, sizeof(frame), &encoded_length)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_COTP_EXCHANGE_FAILED", "Native wire client could not complete the COTP handshake.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_COTP_CONNECTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its cotp-connected state.");
        goto fail;
    }

    if (!unitlab_mms_build_live_wire_association_request_frame(association_request, sizeof(association_request), &association_length, &diagnostic)) {
        set_result(result, "NATIVE_WIRE_CLIENT_FRAME_BUILD_FAILED", diagnostic.message);
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATING;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its associating state.");
        goto fail;
    }
    if (!send_all(data_fd, association_request, association_length) || !read_tpkt_frame(data_fd, association_request, sizeof(association_request), &association_length)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_ASSOCIATION_FAILED", "Native wire client could not complete the association handshake.");
        goto fail;
    }
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_ASSOCIATED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its associated state.");
        goto fail;
    }

    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its read-requested state.");
        goto fail;
    }
    if (!emit_read_response(
            &session,
            data_fd,
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
            frame,
            sizeof(frame),
            &diagnostic)) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
        set_result(result, "NATIVE_WIRE_CLIENT_READ_FAILED", diagnostic.message);
        goto fail;
    }

    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
    if (!emit_state_response(state)) {
        set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state.");
        goto fail;
    }
    if (!emit_text_response("native-wire-client: ready")) {
        set_result(result, "NATIVE_WIRE_CLIENT_READY_FAILED", "Native wire client could not emit its ready banner.");
        goto fail;
    }

    while (stop_requested == NULL || !stop_requested(stop_context)) {
        char command[512U];
        fd_set read_set;
        int max_fd = data_fd;
        int ready;

        FD_ZERO(&read_set);
        if (data_fd >= 0) {
            FD_SET(data_fd, &read_set);
        }
        FD_SET(STDIN_FILENO, &read_set);
        if (STDIN_FILENO > max_fd) {
            max_fd = STDIN_FILENO;
        }
        do {
            ready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        } while (ready < 0 && errno == EINTR);
        if (ready < 0) {
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
            set_result(result, "NATIVE_WIRE_CLIENT_SELECT_FAILED", "Native wire client command/report wait failed.");
            goto fail;
        }
        if (data_fd >= 0 && FD_ISSET(data_fd, &read_set)) {
            int async_frame = emit_async_data_frame_if_ready(&session, data_fd, report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic);
            if (async_frame < 0) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_ASYNC_REPORT_FAILED", diagnostic.message);
                goto fail;
            }
        }
        if (!FD_ISSET(STDIN_FILENO, &read_set)) {
            continue;
        }
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        command[strcspn(command, "\r\n")] = '\0';
        if (strncmp(command, "discover ", 9U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 9U, " 	", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " 	", &saveptr);
            char* extra = strtok_r(NULL, " 	", &saveptr);
            uint32_t invoke_id = next_invoke_id;

            if (domain_id == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_COMMAND_INVALID", "Usage: discover <domain> [invokeBase].");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_INVOKE_INVALID", "Native wire client discover invokeBase must be in range 1..4294967295.");
                    goto fail;
                }
            }
            if (invoke_id > UINT32_MAX - UNITLAB_NATIVE_DISCOVERY_MAX_INVOKE_SPAN) {
                set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_INVOKE_INVALID", "Native wire client discover invokeBase leaves too few invoke IDs for the sequence.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its discover state.");
                goto fail;
            }
            {
                UnitLabNativeDiscoveryIo discovery_io = {
                    .data_fd = data_fd,
                    .scratch = scratch,
                    .scratch_length = sizeof(scratch),
                    .request = read_request,
                    .request_length = sizeof(read_request),
                    .response = report_frame,
                    .response_length = sizeof(report_frame),
                    .encoded_response_length = &report_length,
                    .text_buffer = frame,
                    .text_buffer_length = sizeof(frame),
                    .diagnostic = &diagnostic,
                    .get_name_list_step = discovery_get_name_list_step_adapter,
                    .read_step = discovery_read_step_adapter,
                    .attributes_step = discovery_attributes_step_adapter,
                    .emit_model_summary = emit_discovered_model_summary,
                };
                if (!unitlab_native_client_run_discover_sequence(
                        &session,
                        &discovery_io,
                        domain_id,
                        invoke_id,
                        &next_invoke_id)) {
                    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                    set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                    goto fail;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after discover.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "read ", 5U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 5U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_READ_COMMAND_INVALID", "Usage: read <domain> <item> [invokeId].");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_READ_INVOKE_INVALID", "Native wire client read invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READ_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its read-requested state.");
                goto fail;
            }
            if (!emit_read_response(
                    &session,
                    data_fd,
                    domain_id,
                    item_id,
                    invoke_id,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_READ_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after read.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "get-name-list ", 14U) == 0) {
            char* saveptr = NULL;
            char* class_text = strtok_r(command + 14U, " \t", &saveptr);
            char* scope_text = strtok_r(NULL, " \t", &saveptr);
            char* domain_text = strtok_r(NULL, " \t", &saveptr);
            char* continue_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            const char* domain_id = NULL;
            const char* continue_after = NULL;
            uint32_t object_class = 0U;
            uint32_t object_scope = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (class_text == NULL || scope_text == NULL || domain_text == NULL || continue_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_COMMAND_INVALID", "Usage: get-name-list <class> <scope> <domain|-> <continueAfter|-> [invokeId].");
                goto fail;
            }
            if (!parse_uint32_token(class_text, &object_class) || !parse_uint32_token(scope_text, &object_scope)) {
                set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_ARGUMENT_INVALID", "Native wire client GetNameList class and scope must be unsigned integers.");
                goto fail;
            }
            if (strcmp(domain_text, "-") != 0) {
                domain_id = domain_text;
            }
            if (strcmp(continue_text, "-") != 0) {
                continue_after = continue_text;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_INVOKE_INVALID", "Native wire client GetNameList invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its get-name-list-requested state.");
                goto fail;
            }
            if (!emit_get_name_list_response(
                    &session,
                    data_fd,
                    object_class,
                    object_scope,
                    domain_id,
                    NULL,
                    continue_after,
                    invoke_id,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_GET_NAME_LIST_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after GetNameList.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "get-var-attrs ", 14U) == 0 || strncmp(command, "get-nvl-attrs ", 14U) == 0) {
            int named_variable_list = strncmp(command, "get-nvl-attrs ", 14U) == 0;
            char* saveptr = NULL;
            char* domain_text = strtok_r(command + 14U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            const char* domain_id = NULL;
            uint32_t invoke_id = next_invoke_id++;

            if (domain_text == NULL || item_id == NULL || extra != NULL) {
                set_result(
                    result,
                    named_variable_list ? "NATIVE_WIRE_CLIENT_GET_NVL_ATTRS_COMMAND_INVALID" : "NATIVE_WIRE_CLIENT_GET_VAR_ATTRS_COMMAND_INVALID",
                    named_variable_list ? "Usage: get-nvl-attrs <domain|-> <item> [invokeId]." : "Usage: get-var-attrs <domain|-> <item> [invokeId].");
                goto fail;
            }
            if (strcmp(domain_text, "-") != 0) {
                domain_id = domain_text;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_GET_ATTRS_INVOKE_INVALID", "Native wire client attribute invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_ATTRIBUTES_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its attributes-requested state.");
                goto fail;
            }
            if (!emit_get_attributes_response(
                    &session,
                    data_fd,
                    domain_id,
                    item_id,
                    invoke_id,
                    named_variable_list,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, named_variable_list ? "NATIVE_WIRE_CLIENT_GET_NVL_ATTRS_FAILED" : "NATIVE_WIRE_CLIENT_GET_VAR_ATTRS_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after attribute request.");
                goto fail;
            }
            continue;
        }
        if (strcmp(command, "close-ied") == 0) {
            unitlab_native_client_session_reset(&session);
            emit_discovered_model_summary(&session, "close-ied");
            emit_subscription_summary(&session, "close-ied");
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after close-ied.");
                goto fail;
            }
            continue;
        }
        if (strcmp(command, "connect-ied") == 0) {
            if (session.discovered_model.domain[0] == '\0' || session.discovered_model.logical_node_count == 0U) {
                set_result(result, "NATIVE_WIRE_CLIENT_CONNECT_IED_NO_DEVICE", "Run discover first before connect-ied.");
                goto fail;
            }
            printf("native-wire-client: connect-ied domain=%s\n", session.discovered_model.domain);
            emit_discovered_model_summary(&session, "connect-ied");
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after connect-ied.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "rptena", 6U) == 0 && (command[6] == '\0' || command[6] == ' ' || command[6] == '\t')) {
            char* saveptr = NULL;
            char* index_text = strtok_r(command + 6U, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint32_t rcb_index = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_COMMAND_INVALID", "Usage: rptena [discoveredRcbIndex] [invokeId].");
                goto fail;
            }
            if (index_text != NULL && !parse_uint32_token(index_text, &rcb_index)) {
                set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_INDEX_INVALID", "Native wire client discovered RCB index must be unsigned.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_INVOKE_INVALID", "Native wire client RptEna invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            selected_rcb = unitlab_native_client_session_discovered_rcb_at(&session, rcb_index);
            if (selected_rcb == NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_NO_DISCOVERED_RCB", "Run discover first and select an existing discovered BRCB index.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_discovered_rcb_bool_step(&session, data_fd, "rptena", selected_rcb->domain, selected_rcb->item, "RptEna", 1U, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_FAILED", diagnostic.message);
                goto fail;
            }
            session.subscription_model.rpt_enabled = 1;
            session.subscription_model.selected_rcb_index = rcb_index;
            session.subscription_model.last_rptena_invoke_id = invoke_id;
            snprintf(session.subscription_model.rcb_domain, sizeof(session.subscription_model.rcb_domain), "%s", selected_rcb->domain);
            snprintf(session.subscription_model.rcb_item, sizeof(session.subscription_model.rcb_item), "%s", selected_rcb->item);
            emit_subscription_summary(&session, "rptena");
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after RptEna.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "gi", 2U) == 0 && (command[2] == '\0' || command[2] == ' ' || command[2] == '\t')) {
            char* saveptr = NULL;
            char* index_text = strtok_r(command + 2U, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint32_t rcb_index = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_GI_COMMAND_INVALID", "Usage: gi [discoveredRcbIndex] [invokeId].");
                goto fail;
            }
            if (index_text != NULL && !parse_uint32_token(index_text, &rcb_index)) {
                set_result(result, "NATIVE_WIRE_CLIENT_GI_INDEX_INVALID", "Native wire client discovered RCB index must be unsigned.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_GI_INVOKE_INVALID", "Native wire client GI invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            selected_rcb = unitlab_native_client_session_discovered_rcb_at(&session, rcb_index);
            if (selected_rcb == NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_GI_NO_DISCOVERED_RCB", "Run discover first and select an existing discovered BRCB index.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_discovered_rcb_bool_step(&session, data_fd, "gi", selected_rcb->domain, selected_rcb->item, "GI", 1U, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_GI_FAILED", diagnostic.message);
                goto fail;
            }
            session.subscription_model.gi_requested = 1;
            session.subscription_model.selected_rcb_index = rcb_index;
            session.subscription_model.last_gi_invoke_id = invoke_id;
            snprintf(session.subscription_model.rcb_domain, sizeof(session.subscription_model.rcb_domain), "%s", selected_rcb->domain);
            snprintf(session.subscription_model.rcb_item, sizeof(session.subscription_model.rcb_item), "%s", selected_rcb->item);
            emit_subscription_summary(&session, "gi");
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after GI.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "write-bool ", 11U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 11U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* value_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint8_t boolean_value = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || value_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_COMMAND_INVALID", "Usage: write-bool <domain> <item> <true|false|1|0> [invokeId].");
                goto fail;
            }
            if (!parse_bool_token(value_text, &boolean_value)) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_VALUE_INVALID", "Native wire client boolean value must be true, false, 1, or 0.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_INVOKE_INVALID", "Native wire client write invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_write_bool_response(
                    &session,
                    data_fd,
                    domain_id,
                    item_id,
                    boolean_value,
                    invoke_id,
                    scratch,
                    sizeof(scratch),
                    read_request,
                    sizeof(read_request),
                    report_frame,
                    sizeof(report_frame),
                    &report_length,
                    frame,
                    sizeof(frame),
                    &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_BOOL_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after write.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "write-uint ", 11U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 11U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* tag_text = strtok_r(NULL, " \t", &saveptr);
            char* value_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint8_t value_bytes[4U];
            size_t value_length = 0U;
            uint32_t tag_number = 0U;
            uint32_t value = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || tag_text == NULL || value_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_UINT_COMMAND_INVALID", "Usage: write-uint <domain> <item> <tag> <value> [invokeId].");
                goto fail;
            }
            if (!parse_uint32_token(tag_text, &tag_number) || !parse_uint32_token(value_text, &value) || !encode_uint32_value(value, value_bytes, sizeof(value_bytes), &value_length)) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_UINT_ARGUMENT_INVALID", "Native wire client write-uint tag and value must be unsigned integers.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_WRITE_UINT_INVOKE_INVALID", "Native wire client write invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_write_element_response(&session, data_fd, domain_id, item_id, tag_number, value_bytes, value_length, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_UINT_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after write.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "write-string ", 13U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 13U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* tag_text = strtok_r(NULL, " \t", &saveptr);
            char* value_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint32_t tag_number = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || tag_text == NULL || value_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_STRING_COMMAND_INVALID", "Usage: write-string <domain> <item> <tag> <value> [invokeId].");
                goto fail;
            }
            if (!parse_uint32_token(tag_text, &tag_number)) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_STRING_TAG_INVALID", "Native wire client write-string tag must be an unsigned integer.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_WRITE_STRING_INVOKE_INVALID", "Native wire client write invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_write_element_response(&session, data_fd, domain_id, item_id, tag_number, (const uint8_t*)value_text, strlen(value_text), invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_STRING_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after write.");
                goto fail;
            }
            continue;
        }
        if (strncmp(command, "write-hex ", 10U) == 0) {
            char* saveptr = NULL;
            char* domain_id = strtok_r(command + 10U, " \t", &saveptr);
            char* item_id = strtok_r(NULL, " \t", &saveptr);
            char* tag_text = strtok_r(NULL, " \t", &saveptr);
            char* value_text = strtok_r(NULL, " \t", &saveptr);
            char* invoke_id_text = strtok_r(NULL, " \t", &saveptr);
            char* extra = strtok_r(NULL, " \t", &saveptr);
            uint8_t value_bytes[256U];
            size_t value_length = 0U;
            uint32_t tag_number = 0U;
            uint32_t invoke_id = next_invoke_id++;

            if (domain_id == NULL || item_id == NULL || tag_text == NULL || value_text == NULL || extra != NULL) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_HEX_COMMAND_INVALID", "Usage: write-hex <domain> <item> <tag> <hex> [invokeId].");
                goto fail;
            }
            if (!parse_uint32_token(tag_text, &tag_number) || !decode_hex_value(value_text, value_bytes, sizeof(value_bytes), &value_length)) {
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_HEX_ARGUMENT_INVALID", "Native wire client write-hex tag must be unsigned and value must be even-length hex.");
                goto fail;
            }
            if (invoke_id_text != NULL) {
                if (!parse_invoke_id_token(invoke_id_text, &invoke_id)) {
                    set_result(result, "NATIVE_WIRE_CLIENT_WRITE_HEX_INVOKE_INVALID", "Native wire client write invokeId must be in range 1..4294967295.");
                    goto fail;
                }
                if (invoke_id >= next_invoke_id) {
                    next_invoke_id = invoke_id + 1U;
                }
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_write_element_response(&session, data_fd, domain_id, item_id, tag_number, value_bytes, value_length, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_WRITE_HEX_FAILED", diagnostic.message);
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after write.");
                goto fail;
            }
            continue;
        }
        if (strcmp(command, "emit-report") == 0) {
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_REPORT_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its report-requested state.");
                goto fail;
            }
            if (!send_report_control_command(control_fd, "emit-report")) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_REPORT_COMMAND_FAILED", "Native wire client could not send the report command.");
                goto fail;
            }
            if (!read_tpkt_frame(data_fd, report_frame, sizeof(report_frame), &report_length)) {
                set_result(result, "NATIVE_WIRE_CLIENT_REPORT_FRAME_FAILED", "Native wire client could not receive the report frame.");
                goto fail;
            }
            if (!emit_wire_frame_response(&session, report_frame, report_length, frame, sizeof(frame))) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_RESPONSE_FAILED", "Native wire client could not emit the report frame.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_READY;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its ready state after report.");
                goto fail;
            }
            continue;
        }
        if (strcmp(command, "disconnect") == 0) {
            if (data_fd >= 0) {
                close(data_fd);
                data_fd = -1;
            }
            if (control_fd >= 0) {
                close(control_fd);
                control_fd = -1;
            }
            printf("native-wire-client: disconnected\n");
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its stopped state after disconnect.");
                goto fail;
            }
            set_result(result, "NATIVE_WIRE_CLIENT_DISCONNECTED", "Native wire client disconnected.");
            unitlab_native_client_session_reset(&session);
            return 1;
        }
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0 || strcmp(command, "stop") == 0) {
            break;
        }
    }

    close(data_fd);
    close(control_fd);
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED;
    emit_state_response(state);
    set_result(result, "NATIVE_WIRE_CLIENT_STOPPED", "Native wire client stopped.");
    unitlab_native_client_session_reset(&session);
    return 1;

fail:
    if (data_fd >= 0) {
        close(data_fd);
    }
    if (control_fd >= 0) {
        close(control_fd);
    }
    if (state != UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED) {
        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
    }
    emit_state_response(state);
    unitlab_native_client_session_reset(&session);
    return 0;
}

int unitlab_run_native_wire_client(
    const UnitLabIedServerConfig* config,
    UnitLabIedModelLoadResult* result,
    UnitLabIedServerStopRequested stop_requested,
    void* stop_context)
{
    return unitlab_run_native_wire_client_with_options(config, NULL, result, stop_requested, stop_context);
}
