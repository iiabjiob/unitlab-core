#define _POSIX_C_SOURCE 200112L
#include "native_wire_client.h"

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

static int bytes_are_printable_ascii(const uint8_t* bytes, size_t length)
{
    if (bytes == NULL) {
        return 0;
    }
    for (size_t index = 0U; index < length; index++) {
        if (bytes[index] < 0x20U || bytes[index] > 0x7EU) {
            return 0;
        }
    }
    return 1;
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
    } else if (bytes_are_printable_ascii(value->value_bytes, value->value_length)) {
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
                if (bytes_are_printable_ascii(result.value_bytes, result.value_length)) {
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

static int decode_object_name_domain_item(const UnitLabMmsBerElement* object_name, char* domain, size_t domain_size, char* item, size_t item_size)
{
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement child;
    size_t consumed = 0U;
    size_t offset = 0U;

    if (object_name == NULL || domain == NULL || item == NULL || domain_size == 0U || item_size == 0U) {
        return 0;
    }
    domain[0] = '\0';
    item[0] = '\0';
    unitlab_mms_diagnostic_clear(&diagnostic);
    if (object_name->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && !object_name->tag.constructed && object_name->tag.tag_number == 0U) {
        size_t item_length = object_name->value_length < item_size - 1U ? object_name->value_length : item_size - 1U;
        memcpy(item, object_name->value_bytes, item_length);
        item[item_length] = '\0';
        return 1;
    }
    if (!(object_name->tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && object_name->tag.constructed && object_name->tag.tag_number == 1U)) {
        return 0;
    }
    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, object_name->value_bytes, object_name->value_length, &consumed, &diagnostic)
        || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
        || child.tag.tag_number != 26U) {
        return 0;
    }
    {
        size_t domain_length = child.value_length < domain_size - 1U ? child.value_length : domain_size - 1U;
        memcpy(domain, child.value_bytes, domain_length);
        domain[domain_length] = '\0';
    }
    offset += consumed;
    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, &object_name->value_bytes[offset], object_name->value_length - offset, &consumed, &diagnostic)
        || child.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
        || child.tag.tag_number != 26U) {
        return 0;
    }
    {
        size_t item_length = child.value_length < item_size - 1U ? child.value_length : item_size - 1U;
        memcpy(item, child.value_bytes, item_length);
        item[item_length] = '\0';
    }
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
                && bytes_are_printable_ascii(first_child.value_bytes, first_child.value_length)) {
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
            && decode_object_name_domain_item(&object_name, domain, sizeof(domain), item, sizeof(item))) {
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

static size_t extract_get_name_list_identifiers(
    const uint8_t* frame,
    size_t frame_length,
    char identifiers[][128U],
    size_t max_identifiers,
    int* more_follows,
    char* last_identifier,
    size_t last_identifier_size)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement list_element;
    size_t consumed = 0U;
    size_t offset = 0U;
    size_t count = 0U;

    if (more_follows != NULL) {
        *more_follows = 0;
    }
    if (last_identifier != NULL && last_identifier_size > 0U) {
        last_identifier[0] = '\0';
    }
    if (identifiers != NULL) {
        for (size_t index = 0U; index < max_identifiers; index++) {
            identifiers[index][0] = '\0';
        }
    }
    if (frame == NULL || frame_length == 0U || identifiers == NULL || max_identifiers == 0U) {
        return 0U;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, frame, frame_length, &consumed, &diagnostic)) {
        return 0U;
    }
    unitlab_mms_pdu_init(&pdu);
    if (association_frame.presentation.payload_bytes == NULL
        || association_frame.presentation.payload_length == 0U
        || !unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed, &diagnostic)
        || pdu.kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        || pdu.service_kind != UNITLAB_MMS_SERVICE_GET_NAME_LIST) {
        return 0U;
    }
    unitlab_mms_ber_element_init(&list_element);
    if (!unitlab_mms_ber_read(&list_element, pdu.service_bytes, pdu.service_length, &consumed, &diagnostic)
        || list_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || list_element.tag.tag_number != 0U) {
        return 0U;
    }
    while (offset < list_element.value_length && count < max_identifiers) {
        UnitLabMmsBerElement item_element;
        size_t item_consumed = 0U;
        unitlab_mms_ber_element_init(&item_element);
        if (!unitlab_mms_ber_read(&item_element, &list_element.value_bytes[offset], list_element.value_length - offset, &item_consumed, &diagnostic) || item_consumed == 0U) {
            break;
        }
        if (item_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
            && item_element.tag.tag_number == 26U
            && item_element.value_length > 0U
            && bytes_are_printable_ascii(item_element.value_bytes, item_element.value_length)) {
            size_t copy_length = item_element.value_length < 127U ? item_element.value_length : 127U;
            memcpy(identifiers[count], item_element.value_bytes, copy_length);
            identifiers[count][copy_length] = '\0';
            if (last_identifier != NULL && last_identifier_size > 0U) {
                size_t last_copy_length = item_element.value_length < last_identifier_size - 1U ? item_element.value_length : last_identifier_size - 1U;
                memcpy(last_identifier, item_element.value_bytes, last_copy_length);
                last_identifier[last_copy_length] = '\0';
            }
            count++;
        }
        offset += item_consumed;
    }
    if (consumed < pdu.service_length && more_follows != NULL) {
        UnitLabMmsBerElement more_follows_element;
        size_t more_follows_consumed = 0U;
        unitlab_mms_ber_element_init(&more_follows_element);
        if (unitlab_mms_ber_read(&more_follows_element, &pdu.service_bytes[consumed], pdu.service_length - consumed, &more_follows_consumed, &diagnostic)
            && more_follows_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
            && more_follows_element.tag.tag_number == 1U
            && more_follows_element.value_length > 0U) {
            *more_follows = more_follows_element.value_bytes[0] != 0U;
        }
    }
    return count;
}

static void emit_mms_frame_summary(const uint8_t* frame, size_t frame_length)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed = 0U;

    if (frame == NULL || frame_length == 0U) {
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

static int emit_wire_frame_response(const uint8_t* frame, size_t frame_length, uint8_t* text_buffer, size_t text_buffer_length)
{
    if (!format_hex_response(frame, frame_length, (char*)text_buffer, text_buffer_length) || !emit_text_response((const char*)text_buffer)) {
        return 0;
    }
    emit_mms_frame_summary(frame, frame_length);
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

static int emit_confirmed_response(
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
    if (encoded_response_length == NULL || !emit_wire_frame_response(response, *encoded_response_length, text_buffer, text_buffer_length)) {
        if (diagnostic != NULL) {
            diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL;
            snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", "Native wire client could not format the confirmed response.");
        }
        return 0;
    }
    return 1;
}

static int emit_read_response(
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
    int data_fd,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
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

    if (!unitlab_mms_build_get_name_list_request_frame(
            object_class,
            object_scope,
            domain_id,
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
    int data_fd,
    const char* label,
    uint32_t object_class,
    uint32_t object_scope,
    const char* domain_id,
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
        "native-wire-client: discover-step=%s invoke=%u class=%u scope=%u domain=%s continue-after=%s\n",
        label != NULL ? label : "get-name-list",
        (unsigned)invoke_id,
        (unsigned)object_class,
        (unsigned)object_scope,
        domain_id != NULL ? domain_id : "<none>",
        continue_after != NULL ? continue_after : "<none>");
    fflush(stdout);
    return emit_get_name_list_response(
        data_fd,
        object_class,
        object_scope,
        domain_id,
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

static int emit_get_attributes_response(
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
            if (encoded_response_length == NULL || !emit_wire_frame_response(response, *encoded_response_length, text_buffer, text_buffer_length)) {
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
            if (encoded_response_length == NULL || !emit_wire_frame_response(response, *encoded_response_length, text_buffer, text_buffer_length)) {
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
    char discovered_domain[128U];
    char discovered_brcb_items[4U][320U];
    size_t discovered_brcb_count = 0U;
    UnitLabMmsDiagnostic diagnostic;
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
    discovered_domain[0] = '\0';
    memset(discovered_brcb_items, 0, sizeof(discovered_brcb_items));
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
            if (invoke_id > UINT32_MAX - 40U) {
                set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_INVOKE_INVALID", "Native wire client discover invokeBase leaves too few invoke IDs for the sequence.");
                goto fail;
            }
            next_invoke_id = invoke_id + 41U;
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_GET_NAME_LIST_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its discover state.");
                goto fail;
            }
            {
                char logical_node_names[4U][128U];
                char data_set_items[4U][128U];
                char brcb_names[4U][128U];
                char brcb_logical_nodes[4U][128U];
                size_t logical_node_count = 0U;
                size_t data_set_count = 0U;
                size_t brcb_count = 0U;
                uint32_t followup_invoke_id = invoke_id + 2U;
                int more_follows = 0;
                char last_identifier[128U];

                discovered_domain[0] = '\0';
                memset(discovered_brcb_items, 0, sizeof(discovered_brcb_items));
                discovered_brcb_count = 0U;

                if (!emit_discover_get_name_list_step(data_fd, "vmd-logical-devices", 9U, 0U, NULL, NULL, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)
                    || !emit_discover_get_name_list_step(data_fd, "domain-logical-nodes", 1U, 1U, domain_id, NULL, invoke_id + 1U, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                    set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                    goto fail;
                }
                logical_node_count = extract_get_name_list_identifiers(report_frame, report_length, logical_node_names, 4U, &more_follows, last_identifier, sizeof(last_identifier));
                while (more_follows && logical_node_count < 4U && last_identifier[0] != '\0') {
                    char page_items[4U][128U];
                    size_t page_count;
                    if (!emit_discover_get_name_list_step(data_fd, "domain-logical-nodes-page", 1U, 1U, domain_id, last_identifier, followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                    page_count = extract_get_name_list_identifiers(report_frame, report_length, page_items, 4U, &more_follows, last_identifier, sizeof(last_identifier));
                    for (size_t page_index = 0U; page_index < page_count && logical_node_count < 4U; page_index++) {
                        snprintf(logical_node_names[logical_node_count], sizeof(logical_node_names[logical_node_count]), "%s", page_items[page_index]);
                        logical_node_count++;
                    }
                }
                if (more_follows) {
                    printf("native-wire-client: discover-truncated=logical-nodes limit=4 continue-after=%s\n", last_identifier[0] != '\0' ? last_identifier : "<none>");
                    fflush(stdout);
                }
                if (!emit_discover_get_name_list_step(data_fd, "domain-datasets", 2U, 1U, domain_id, NULL, followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                    set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                    goto fail;
                }
                data_set_count = extract_get_name_list_identifiers(report_frame, report_length, data_set_items, 4U, &more_follows, last_identifier, sizeof(last_identifier));
                while (more_follows && data_set_count < 4U && last_identifier[0] != '\0') {
                    char page_items[4U][128U];
                    size_t page_count;
                    if (!emit_discover_get_name_list_step(data_fd, "domain-datasets-page", 2U, 1U, domain_id, last_identifier, followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                    page_count = extract_get_name_list_identifiers(report_frame, report_length, page_items, 4U, &more_follows, last_identifier, sizeof(last_identifier));
                    for (size_t page_index = 0U; page_index < page_count && data_set_count < 4U; page_index++) {
                        snprintf(data_set_items[data_set_count], sizeof(data_set_items[data_set_count]), "%s", page_items[page_index]);
                        data_set_count++;
                    }
                }
                if (more_follows) {
                    printf("native-wire-client: discover-truncated=datasets limit=4 continue-after=%s\n", last_identifier[0] != '\0' ? last_identifier : "<none>");
                    fflush(stdout);
                }
                if (logical_node_count == 0U) {
                    snprintf(logical_node_names[0], sizeof(logical_node_names[0]), "%s", "LLN0");
                    logical_node_count = 1U;
                    printf("native-wire-client: discover-fallback=logical-nodes value=LLN0\n");
                    fflush(stdout);
                }
                for (size_t ln_index = 0U; ln_index < logical_node_count; ln_index++) {
                    char step_label[160U];
                    char ln_brcb_names[4U][128U];
                    size_t ln_brcb_count = 0U;

                    snprintf(step_label, sizeof(step_label), "ln-data-attributes:%s", logical_node_names[ln_index]);
                    if (!emit_discover_get_name_list_step(data_fd, step_label, 3U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                    snprintf(step_label, sizeof(step_label), "ln-brcbs:%s", logical_node_names[ln_index]);
                    if (!emit_discover_get_name_list_step(data_fd, step_label, 4U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                    ln_brcb_count = extract_get_name_list_identifiers(report_frame, report_length, ln_brcb_names, 4U, &more_follows, last_identifier, sizeof(last_identifier));
                    if (more_follows) {
                        printf("native-wire-client: discover-truncated=ln-brcbs:%s limit=4 continue-after=%s\n", logical_node_names[ln_index], last_identifier[0] != '\0' ? last_identifier : "<none>");
                        fflush(stdout);
                    }
                    for (size_t brcb_index = 0U; brcb_index < ln_brcb_count && brcb_count < 4U; brcb_index++) {
                        snprintf(brcb_names[brcb_count], sizeof(brcb_names[brcb_count]), "%s", ln_brcb_names[brcb_index]);
                        snprintf(brcb_logical_nodes[brcb_count], sizeof(brcb_logical_nodes[brcb_count]), "%s", logical_node_names[ln_index]);
                        brcb_count++;
                    }
                    snprintf(step_label, sizeof(step_label), "ln-urcbs:%s", logical_node_names[ln_index]);
                    if (!emit_discover_get_name_list_step(data_fd, step_label, 5U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                }
                if (brcb_count == 0U) {
                    printf("native-wire-client: discover-skip=brcb-attrs reason=no-brcb\n");
                    fflush(stdout);
                }
                for (size_t index = 0U; index < brcb_count; index++) {
                    char brcb_item[320U];
                    char brcb_read_item[320U];
                    snprintf(brcb_item, sizeof(brcb_item), "%s$BR$%s$RptEna", brcb_logical_nodes[index], brcb_names[index]);
                    if (!emit_discover_attributes_step(data_fd, "brcb-attrs", domain_id, brcb_item, followup_invoke_id++, 0, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                    snprintf(brcb_read_item, sizeof(brcb_read_item), "%s$BR$%s", brcb_logical_nodes[index], brcb_names[index]);
                    if (discovered_brcb_count < 4U) {
                        snprintf(discovered_domain, sizeof(discovered_domain), "%s", domain_id);
                        snprintf(discovered_brcb_items[discovered_brcb_count], sizeof(discovered_brcb_items[discovered_brcb_count]), "%s", brcb_read_item);
                        printf("native-wire-client: discovered-brcb[%zu] domain=%s item=%s\n", discovered_brcb_count, discovered_domain, discovered_brcb_items[discovered_brcb_count]);
                        fflush(stdout);
                        discovered_brcb_count++;
                    }
                    if (!emit_discover_read_step(data_fd, "brcb-values", domain_id, brcb_read_item, followup_invoke_id++, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
                }
                if (data_set_count == 0U) {
                    printf("native-wire-client: discover-skip=dataset-members reason=no-dataset\n");
                    fflush(stdout);
                }
                for (size_t index = 0U; index < data_set_count; index++) {
                    if (!emit_discover_attributes_step(data_fd, "dataset-members", domain_id, data_set_items[index], followup_invoke_id++, 1, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                        state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                        set_result(result, "NATIVE_WIRE_CLIENT_DISCOVER_FAILED", diagnostic.message);
                        goto fail;
                    }
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
                    data_fd,
                    object_class,
                    object_scope,
                    domain_id,
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
            if (discovered_brcb_count == 0U || rcb_index >= discovered_brcb_count) {
                set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_NO_DISCOVERED_RCB", "Run discover first and select an existing discovered BRCB index.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_discovered_rcb_bool_step(data_fd, "rptena", discovered_domain, discovered_brcb_items[rcb_index], "RptEna", 1U, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_RPTENA_FAILED", diagnostic.message);
                goto fail;
            }
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
            if (discovered_brcb_count == 0U || rcb_index >= discovered_brcb_count) {
                set_result(result, "NATIVE_WIRE_CLIENT_GI_NO_DISCOVERED_RCB", "Run discover first and select an existing discovered BRCB index.");
                goto fail;
            }
            state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_WRITE_REQUESTED;
            if (!emit_state_response(state)) {
                set_result(result, "NATIVE_WIRE_CLIENT_STATE_FAILED", "Native wire client could not emit its write-requested state.");
                goto fail;
            }
            if (!emit_discovered_rcb_bool_step(data_fd, "gi", discovered_domain, discovered_brcb_items[rcb_index], "GI", 1U, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
                state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_FAILED;
                set_result(result, "NATIVE_WIRE_CLIENT_GI_FAILED", diagnostic.message);
                goto fail;
            }
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
            if (!emit_write_element_response(data_fd, domain_id, item_id, tag_number, value_bytes, value_length, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
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
            if (!emit_write_element_response(data_fd, domain_id, item_id, tag_number, (const uint8_t*)value_text, strlen(value_text), invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
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
            if (!emit_write_element_response(data_fd, domain_id, item_id, tag_number, value_bytes, value_length, invoke_id, scratch, sizeof(scratch), read_request, sizeof(read_request), report_frame, sizeof(report_frame), &report_length, frame, sizeof(frame), &diagnostic)) {
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
            if (!emit_wire_frame_response(report_frame, report_length, frame, sizeof(frame))) {
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
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0 || strcmp(command, "stop") == 0) {
            break;
        }
    }

    close(data_fd);
    close(control_fd);
    state = UNITLAB_NATIVE_WIRE_CLIENT_STATE_STOPPED;
    emit_state_response(state);
    set_result(result, "NATIVE_WIRE_CLIENT_STOPPED", "Native wire client stopped.");
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
