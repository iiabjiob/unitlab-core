#include "native_wire_client_discovery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_wire_client_ber_helpers.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"

typedef struct {
    char (*items)[128U];
    size_t count;
    size_t capacity;
} UnitLabNativeIdentifierList;

static void identifier_list_reset(UnitLabNativeIdentifierList* list)
{
    if (list == NULL) {
        return;
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static int identifier_list_append(UnitLabNativeIdentifierList* list, const char* value)
{
    char (*resized)[128U];
    size_t new_capacity;

    if (list == NULL || value == NULL || value[0] == '\0') {
        return 0;
    }
    if (list->count >= list->capacity) {
        new_capacity = list->capacity != 0U ? list->capacity * 2U : 32U;
        if (new_capacity <= list->capacity) {
            return 0;
        }
        resized = (char (*)[128U])realloc(list->items, new_capacity * sizeof(list->items[0]));
        if (resized == NULL) {
            return 0;
        }
        memset(&resized[list->capacity], 0, (new_capacity - list->capacity) * sizeof(resized[0]));
        list->items = resized;
        list->capacity = new_capacity;
    }
    snprintf(list->items[list->count], sizeof(list->items[list->count]), "%s", value);
    list->count++;
    return 1;
}

static void set_discovery_diagnostic(const UnitLabNativeDiscoveryIo* io, UnitLabMmsDiagnosticCode code, const char* message)
{
    if (io == NULL || io->diagnostic == NULL) {
        return;
    }
    io->diagnostic->code = code;
    snprintf(io->diagnostic->message, sizeof(io->diagnostic->message), "%s", message != NULL ? message : "Native wire discovery failed.");
}

static int collect_get_named_variable_list_members_from_frame(UnitLabNativeClientSessionState* session, const UnitLabNativeDiscoveryIo* io, const char* data_set_reference, const uint8_t* frame, size_t frame_length)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement deletable;
    UnitLabMmsBerElement list;
    size_t consumed = 0U;
    size_t offset = 0U;
    size_t added = 0U;
    UnitLabNativeDiscoveredDataSet* data_set = NULL;

    if (session == NULL || data_set_reference == NULL || data_set_reference[0] == '\0' || frame == NULL || frame_length == 0U) {
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
        || pdu.service_kind != UNITLAB_MMS_SERVICE_GET_NAMED_VARIABLE_LIST_ATTRIBUTES) {
        return 0U;
    }
    unitlab_mms_ber_element_init(&deletable);
    if (!unitlab_mms_ber_read(&deletable, pdu.service_bytes, pdu.service_length, &consumed, &diagnostic)) {
        return 0U;
    }
    unitlab_mms_ber_element_init(&list);
    if (!unitlab_mms_ber_read(&list, &pdu.service_bytes[consumed], pdu.service_length - consumed, &consumed, &diagnostic)
        || !(list.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC && list.tag.constructed && list.tag.tag_number == 1U)) {
        return 0U;
    }
    data_set = unitlab_native_client_session_append_data_set(session, data_set_reference);
    if (data_set == NULL) {
        set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered DataSet state.");
        return 0;
    }
    while (offset < list.value_length) {
        UnitLabMmsBerElement member;
        UnitLabMmsBerElement variable_spec;
        UnitLabMmsBerElement object_name;
        char domain[128U];
        char item[256U];
        char reference[384U];
        size_t member_consumed = 0U;
        size_t nested_consumed = 0U;

        unitlab_mms_ber_element_init(&member);
        if (!unitlab_mms_ber_read(&member, &list.value_bytes[offset], list.value_length - offset, &member_consumed, &diagnostic) || member_consumed == 0U) {
            break;
        }
        if (member.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
            && member.tag.constructed
            && member.tag.tag_number == 16U
            && unitlab_mms_ber_read(&variable_spec, member.value_bytes, member.value_length, &nested_consumed, &diagnostic)
            && variable_spec.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
            && variable_spec.tag.constructed
            && variable_spec.tag.tag_number == 0U
            && unitlab_mms_ber_read(&object_name, variable_spec.value_bytes, variable_spec.value_length, &nested_consumed, &diagnostic)
            && unitlab_native_client_decode_object_name_domain_item(&object_name, domain, sizeof(domain), item, sizeof(item))) {
            snprintf(reference, sizeof(reference), "%s/%s", domain[0] != '\0' ? domain : "<vmd>", item);
            if (!unitlab_native_client_session_append_data_set_member(session, data_set, reference)) {
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered DataSet member state.");
                return 0;
            }
            if (unitlab_native_client_session_append_leaf_ref(session, reference) == NULL) {
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered leaf reference state.");
                return 0;
            }
            printf(
                "native-wire-client: discovered-dataset-member[%zu.%zu] dataset=%s ref=%s\n",
                session->discovered_data_set_count - 1U,
                data_set->member_count - 1U,
                data_set->reference,
                session->discovered_data_set_members[session->discovered_data_set_member_count - 1U]);
            fflush(stdout);
            added++;
        }
        offset += member_consumed;
    }
    (void)added;
    return 1;
}



static const char* gva_type_kind_label_for_context_number(uint32_t tag_number)
{
    switch (tag_number) {
        case 0U: return "array";
        case 1U: return "structure";
        case 2U: return "structure";
        case 3U: return "boolean";
        case 4U: return "bit-string";
        case 5U: return "integer";
        case 6U: return "unsigned";
        case 7U: return "float";
        case 8U: return "octet-string";
        case 9U: return "visible-string";
        case 10U: return "generalized-time";
        case 11U: return "binary-time";
        case 12U: return "bcd";
        case 13U: return "boolean-array";
        case 14U: return "obj-id";
        default: return "unknown";
    }
}

static const char* gva_type_kind_label_from_type_spec(const UnitLabMmsBerElement* type_spec)
{
    UnitLabMmsBerElement nested;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed = 0U;

    if (type_spec == NULL || type_spec->tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        return "unknown";
    }
    if (type_spec->tag.tag_number != 1U) {
        return gva_type_kind_label_for_context_number(type_spec->tag.tag_number);
    }
    if (!type_spec->tag.constructed || type_spec->value_bytes == NULL || type_spec->value_length == 0U) {
        return "structure";
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&nested);
    if (!unitlab_mms_ber_read(&nested, type_spec->value_bytes, type_spec->value_length, &consumed, &diagnostic) || consumed == 0U) {
        return "structure";
    }
    if (nested.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC) {
        return gva_type_kind_label_for_context_number(nested.tag.tag_number);
    }
    return "structure";
}

static const char* gva_type_kind_from_bytes(const uint8_t* bytes, size_t length)
{
    UnitLabMmsBerElement element;
    UnitLabMmsDiagnostic diagnostic;
    size_t consumed = 0U;

    if (bytes == NULL || length == 0U) {
        return "unknown";
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_ber_element_init(&element);
    if (!unitlab_mms_ber_read(&element, bytes, length, &consumed, &diagnostic) || consumed == 0U) {
        return "unknown";
    }
    return gva_type_kind_label_from_type_spec(&element);
}

static int build_gva_path(const char* prefix, const char* component_name, char* output, size_t output_size)
{
    int written = 0;

    if (output == NULL || output_size == 0U || component_name == NULL || component_name[0] == '\0') {
        return 0;
    }
    if (prefix != NULL && prefix[0] != '\0') {
        written = snprintf(output, output_size, "%s.%s", prefix, component_name);
    } else {
        written = snprintf(output, output_size, "%s", component_name);
    }
    return written > 0 && (size_t)written < output_size;
}

static int build_gva_reference_fields(
    const UnitLabNativeDiscoveredDataName* data_name,
    const char* fc,
    const char* path,
    char* mms_reference,
    size_t mms_reference_size,
    char* display_reference,
    size_t display_reference_size)
{
    if (mms_reference != NULL && mms_reference_size > 0U) {
        mms_reference[0] = '\0';
    }
    if (display_reference != NULL && display_reference_size > 0U) {
        display_reference[0] = '\0';
    }
    if (data_name == NULL || path == NULL || path[0] == '\0' || fc == NULL || fc[0] == '\0') {
        return 1;
    }
    if (mms_reference != NULL && mms_reference_size > 0U) {
        snprintf(mms_reference, mms_reference_size, "%s/%s$%s$%s", data_name->logical_device, data_name->logical_node, fc, path);
    }
    if (display_reference != NULL && display_reference_size > 0U) {
        snprintf(display_reference, display_reference_size, "%s/%s.%s.%s", data_name->logical_device, data_name->logical_node, fc, path);
    }
    return 1;
}

static void derive_gva_fc_context(const char* item_id, const char* component_name, char* fc, size_t fc_size)
{
    const char* first_dollar = NULL;
    const char* second_dollar = NULL;
    size_t fc_length = 0U;

    if (fc == NULL || fc_size == 0U) {
        return;
    }
    fc[0] = '\0';
    if (item_id != NULL) {
        first_dollar = strchr(item_id, '$');
        if (first_dollar != NULL) {
            second_dollar = strchr(first_dollar + 1, '$');
            if (second_dollar != NULL && second_dollar > first_dollar + 1) {
                fc_length = (size_t)(second_dollar - first_dollar - 1);
                if (fc_length >= fc_size) {
                    fc_length = fc_size - 1U;
                }
                memcpy(fc, first_dollar + 1, fc_length);
                fc[fc_length] = '\0';
                return;
            }
        }
    }
    if (component_name == NULL) {
        return;
    }
    if (strcmp(component_name, "BR") == 0 || strcmp(component_name, "RP") == 0 || strcmp(component_name, "ST") == 0 || strcmp(component_name, "MX") == 0 || strcmp(component_name, "CF") == 0 || strcmp(component_name, "DC") == 0 || strcmp(component_name, "EX") == 0) {
        snprintf(fc, fc_size, "%s", component_name);
    }
}

static int collect_gva_components_from_bytes(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    UnitLabNativeDiscoveredDataName* data_name,
    const char* item_id,
    const char* fc,
    const char* path_prefix,
    size_t parent_index,
    const uint8_t* bytes,
    size_t length,
    size_t depth,
    size_t* component_count)
{
    UnitLabMmsDiagnostic diagnostic;
    size_t offset = 0U;

    if (session == NULL || data_name == NULL || bytes == NULL || component_count == NULL || depth > 16U) {
        return 1;
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
            char component_name[128U];
            char child_path[192U];
            char mms_reference[384U];
            char display_reference[384U];
            const char* component_type_kind = "unknown";
            UnitLabNativeDiscoveredTypedDataNode* node;
            size_t node_index;

            unitlab_mms_ber_element_init(&first_child);
            if (unitlab_mms_ber_read(&first_child, element.value_bytes, element.value_length, &child_consumed, &diagnostic)
                && first_child.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
                && !first_child.tag.constructed
                && first_child.tag.tag_number == 0U
                && first_child.value_length > 0U
                && unitlab_native_client_bytes_are_printable_ascii(first_child.value_bytes, first_child.value_length)) {
                size_t copy_length = first_child.value_length < sizeof(component_name) - 1U ? first_child.value_length : sizeof(component_name) - 1U;
                memcpy(component_name, first_child.value_bytes, copy_length);
                component_name[copy_length] = '\0';
                if (child_consumed < element.value_length) {
                    component_type_kind = gva_type_kind_from_bytes(&element.value_bytes[child_consumed], element.value_length - child_consumed);
                }
                if (!build_gva_path(path_prefix, component_name, child_path, sizeof(child_path))) {
                    set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not build GVA path.");
                    return 0;
                }
                if (!build_gva_reference_fields(data_name, fc, child_path, mms_reference, sizeof(mms_reference), display_reference, sizeof(display_reference))) {
                    set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not build GVA reference fields.");
                    return 0;
                }
                node_index = session->discovered_typed_data_node_count;
                node = unitlab_native_client_session_append_typed_data_node(
                    session,
                    data_name->logical_device,
                    data_name->logical_node,
                    fc,
                    child_path,
                    mms_reference,
                    display_reference,
                    component_type_kind,
                    "branch",
                    depth,
                    parent_index);
                if (node == NULL) {
                    set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate typed GVA tree node state.");
                    return 0;
                }
                if (!unitlab_native_client_session_append_data_component(session, data_name, component_name, component_type_kind)) {
                    set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered data component state.");
                    return 0;
                }
                (*component_count)++;
                if (child_consumed < element.value_length
                    && !collect_gva_components_from_bytes(session, io, data_name, item_id, fc, child_path, node_index, &element.value_bytes[child_consumed], element.value_length - child_consumed, depth + 1U, component_count)) {
                    return 0;
                }
                if (node->child_count == 0U) {
                    snprintf(node->node_kind, sizeof(node->node_kind), "%s", "leaf");
                    if (mms_reference[0] != '\0') {
                        UnitLabNativeDiscoveredLeafRef* leaf_ref = unitlab_native_client_session_append_leaf_ref(session, mms_reference);
                        if (leaf_ref == NULL) {
                            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered leaf reference state.");
                            return 0;
                        }
                        snprintf(leaf_ref->type_kind, sizeof(leaf_ref->type_kind), "%s", component_type_kind);
                    }
                } else {
                    snprintf(node->node_kind, sizeof(node->node_kind), "%s", "branch");
                }
                if (node->mms_reference[0] == '\0' && mms_reference[0] != '\0') {
                    snprintf(node->mms_reference, sizeof(node->mms_reference), "%s", mms_reference);
                }
                if (node->display_reference[0] == '\0' && display_reference[0] != '\0') {
                    snprintf(node->display_reference, sizeof(node->display_reference), "%s", display_reference);
                }
                if (node->type_kind[0] == '\0') {
                    snprintf(node->type_kind, sizeof(node->type_kind), "%s", component_type_kind);
                }
            } else if (!collect_gva_components_from_bytes(session, io, data_name, item_id, fc, path_prefix, parent_index, element.value_bytes, element.value_length, depth + 1U, component_count)) {
                return 0;
            }
        } else if (element.tag.constructed) {
            if (!collect_gva_components_from_bytes(session, io, data_name, item_id, fc, path_prefix, parent_index, element.value_bytes, element.value_length, depth + 1U, component_count)) {
                return 0;
            }
        }
        offset += consumed;
    }
    return 1;
}

static int collect_get_variable_access_attributes_components_from_frame(UnitLabNativeClientSessionState* session, const UnitLabNativeDiscoveryIo* io, UnitLabNativeDiscoveredDataName* data_name, const char* item_id, const uint8_t* frame, size_t frame_length)
{
    UnitLabMmsAssociationFrame association_frame;
    UnitLabMmsPdu pdu;
    UnitLabMmsDiagnostic diagnostic;
    UnitLabMmsBerElement mms_deletable;
    size_t consumed = 0U;
    size_t component_count = 0U;
    char fc[32U];
    char root_path[192U];
    char root_mms_reference[384U];
    char root_display_reference[384U];
    UnitLabNativeDiscoveredTypedDataNode* root_node = NULL;
    const char* root_type_kind;

    if (session == NULL || data_name == NULL || frame == NULL || frame_length == 0U) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, frame, frame_length, &consumed, &diagnostic)) {
        return 0;
    }
    unitlab_mms_pdu_init(&pdu);
    if (association_frame.presentation.payload_bytes == NULL
        || association_frame.presentation.payload_length == 0U
        || !unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed, &diagnostic)
        || pdu.kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        || pdu.service_kind != UNITLAB_MMS_SERVICE_GET_VARIABLE_ACCESS_ATTRIBUTES) {
        return 0;
    }
    unitlab_mms_ber_element_init(&mms_deletable);
    if (!unitlab_mms_ber_read(&mms_deletable, pdu.service_bytes, pdu.service_length, &consumed, &diagnostic) || consumed >= pdu.service_length) {
        return 1;
    }
    root_type_kind = gva_type_kind_from_bytes(&pdu.service_bytes[consumed], pdu.service_length - consumed);
    unitlab_native_client_session_set_data_name_type(data_name, root_type_kind);
    derive_gva_fc_context(item_id, data_name->name, fc, sizeof(fc));
    snprintf(root_path, sizeof(root_path), "%s", data_name->name);
    if (fc[0] != '\0') {
        if (!build_gva_reference_fields(data_name, fc, root_path, root_mms_reference, sizeof(root_mms_reference), root_display_reference, sizeof(root_display_reference))) {
            return 0;
        }
        root_node = unitlab_native_client_session_append_typed_data_node(
            session,
            data_name->logical_device,
            data_name->logical_node,
            fc,
            root_path,
            root_mms_reference,
            root_display_reference,
            root_type_kind,
            "root",
            0U,
            (size_t)-1);
    } else {
        root_node = unitlab_native_client_session_append_typed_data_node(
            session,
            data_name->logical_device,
            data_name->logical_node,
            "",
            root_path,
            NULL,
            NULL,
            root_type_kind,
            "root",
            0U,
            (size_t)-1);
    }
    if (root_node == NULL) {
        set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate typed GVA root node state.");
        return 0;
    }
    if (!collect_gva_components_from_bytes(session, io, data_name, item_id, fc, root_path, session->discovered_typed_data_node_count - 1U, &pdu.service_bytes[consumed], pdu.service_length - consumed, 0U, &component_count)) {
        return 0;
    }
    if (root_node->child_count == 0U && root_mms_reference[0] != '\0') {
        UnitLabNativeDiscoveredLeafRef* leaf_ref = unitlab_native_client_session_append_leaf_ref(session, root_mms_reference);
        if (leaf_ref == NULL) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate typed root leaf reference state.");
            return 0;
        }
        snprintf(leaf_ref->type_kind, sizeof(leaf_ref->type_kind), "%s", root_type_kind);
    }
    printf(
        "native-wire-client: discovered-data-components data=%s/%s$%s count=%zu\n",
        data_name->logical_device,
        data_name->logical_node,
        data_name->name,
        component_count);
    fflush(stdout);
    return 1;
}

static int extract_get_name_list_identifiers(
    const uint8_t* frame,
    size_t frame_length,
    UnitLabNativeIdentifierList* identifiers,
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

    if (more_follows != NULL) {
        *more_follows = 0;
    }
    if (last_identifier != NULL && last_identifier_size > 0U) {
        last_identifier[0] = '\0';
    }
    if (frame == NULL || frame_length == 0U || identifiers == NULL) {
        return 0;
    }
    unitlab_mms_diagnostic_clear(&diagnostic);
    unitlab_mms_association_frame_init(&association_frame);
    if (!unitlab_mms_association_frame_decode(&association_frame, frame, frame_length, &consumed, &diagnostic)) {
        return 0;
    }
    unitlab_mms_pdu_init(&pdu);
    if (association_frame.presentation.payload_bytes == NULL
        || association_frame.presentation.payload_length == 0U
        || !unitlab_mms_pdu_decode(&pdu, association_frame.presentation.payload_bytes, association_frame.presentation.payload_length, &consumed, &diagnostic)
        || pdu.kind != UNITLAB_MMS_PDU_CONFIRMED_RESPONSE
        || pdu.service_kind != UNITLAB_MMS_SERVICE_GET_NAME_LIST) {
        return 0;
    }
    unitlab_mms_ber_element_init(&list_element);
    if (!unitlab_mms_ber_read(&list_element, pdu.service_bytes, pdu.service_length, &consumed, &diagnostic)
        || list_element.tag.tag_class != UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC
        || list_element.tag.tag_number != 0U) {
        return 0;
    }
    while (offset < list_element.value_length) {
        UnitLabMmsBerElement item_element;
        char identifier[128U];
        size_t item_consumed = 0U;
        unitlab_mms_ber_element_init(&item_element);
        if (!unitlab_mms_ber_read(&item_element, &list_element.value_bytes[offset], list_element.value_length - offset, &item_consumed, &diagnostic) || item_consumed == 0U) {
            break;
        }
        if (item_element.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL
            && item_element.tag.tag_number == 26U
            && item_element.value_length > 0U
            && unitlab_native_client_bytes_are_printable_ascii(item_element.value_bytes, item_element.value_length)) {
            size_t copy_length = item_element.value_length < sizeof(identifier) - 1U ? item_element.value_length : sizeof(identifier) - 1U;
            memcpy(identifier, item_element.value_bytes, copy_length);
            identifier[copy_length] = '\0';
            if (!identifier_list_append(identifiers, identifier)) {
                return 0;
            }
            if (last_identifier != NULL && last_identifier_size > 0U) {
                size_t last_copy_length = item_element.value_length < last_identifier_size - 1U ? item_element.value_length : last_identifier_size - 1U;
                memcpy(last_identifier, item_element.value_bytes, last_copy_length);
                last_identifier[last_copy_length] = '\0';
            }
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
    return 1;
}

int unitlab_native_client_run_discover_sequence(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* domain_id,
    uint32_t invoke_id,
    uint32_t* next_invoke_id)
{
    UnitLabNativeIdentifierList logical_device_names = {0};
    UnitLabNativeIdentifierList logical_node_names = {0};
    UnitLabNativeIdentifierList data_set_items = {0};
    UnitLabNativeIdentifierList brcb_names = {0};
    UnitLabNativeIdentifierList brcb_logical_nodes = {0};
    uint32_t followup_invoke_id = invoke_id + 2U;
    int more_follows = 0;
    char last_identifier[128U];
    int ok = 0;

    if (session == NULL || io == NULL || io->get_name_list_step == NULL || io->read_step == NULL || io->attributes_step == NULL || domain_id == NULL || domain_id[0] == '\0' || next_invoke_id == NULL) {
        set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Native wire client discover sequence requires session, domain, and discovery callbacks.");
        return 0;
    }

    unitlab_native_client_session_reset(session);
    snprintf(session->discovered_model.domain, sizeof(session->discovered_model.domain), "%s", domain_id);

    if (!io->get_name_list_step(session, io, "vmd-logical-devices", 9U, 0U, NULL, NULL, NULL, invoke_id)) {
        goto cleanup;
    }
    if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &logical_device_names, &more_follows, last_identifier, sizeof(last_identifier))) {
        set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode logical device GetNameList response.");
        goto cleanup;
    }
    while (more_follows && last_identifier[0] != '\0') {
        size_t before_count = logical_device_names.count;
        if (!io->get_name_list_step(session, io, "vmd-logical-devices-page", 9U, 0U, NULL, NULL, last_identifier, followup_invoke_id++)) {
            goto cleanup;
        }
        if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &logical_device_names, &more_follows, last_identifier, sizeof(last_identifier))) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode logical device GetNameList page.");
            goto cleanup;
        }
        if (logical_device_names.count == before_count) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client logical device pagination did not advance.");
            goto cleanup;
        }
    }
    for (size_t index = 0U; index < logical_device_names.count; index++) {
        if (unitlab_native_client_session_append_logical_device(session, logical_device_names.items[index]) == NULL) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered logical device state.");
            goto cleanup;
        }
    }

    if (!io->get_name_list_step(session, io, "domain-logical-nodes", 1U, 1U, domain_id, NULL, NULL, invoke_id + 1U)) {
        goto cleanup;
    }
    if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &logical_node_names, &more_follows, last_identifier, sizeof(last_identifier))) {
        set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode logical node GetNameList response.");
        goto cleanup;
    }
    while (more_follows && last_identifier[0] != '\0') {
        size_t before_count = logical_node_names.count;
        if (!io->get_name_list_step(session, io, "domain-logical-nodes-page", 1U, 1U, domain_id, NULL, last_identifier, followup_invoke_id++)) {
            goto cleanup;
        }
        if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &logical_node_names, &more_follows, last_identifier, sizeof(last_identifier))) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode logical node GetNameList page.");
            goto cleanup;
        }
        if (logical_node_names.count == before_count) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client logical node pagination did not advance.");
            goto cleanup;
        }
    }

    if (!io->get_name_list_step(session, io, "domain-datasets", 2U, 1U, domain_id, NULL, NULL, followup_invoke_id++)) {
        goto cleanup;
    }
    if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &data_set_items, &more_follows, last_identifier, sizeof(last_identifier))) {
        set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode dataset GetNameList response.");
        goto cleanup;
    }
    while (more_follows && last_identifier[0] != '\0') {
        size_t before_count = data_set_items.count;
        if (!io->get_name_list_step(session, io, "domain-datasets-page", 2U, 1U, domain_id, NULL, last_identifier, followup_invoke_id++)) {
            goto cleanup;
        }
        if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &data_set_items, &more_follows, last_identifier, sizeof(last_identifier))) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode dataset GetNameList page.");
            goto cleanup;
        }
        if (data_set_items.count == before_count) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client dataset pagination did not advance.");
            goto cleanup;
        }
    }
    session->discovered_model.data_set_count = data_set_items.count;

    if (logical_node_names.count == 0U) {
        if (!identifier_list_append(&logical_node_names, "LLN0")) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate logical node fallback.");
            goto cleanup;
        }
        printf("native-wire-client: discover-fallback=logical-nodes value=LLN0\n");
        fflush(stdout);
    }
    for (size_t index = 0U; index < logical_node_names.count; index++) {
        if (unitlab_native_client_session_append_logical_node(session, domain_id, logical_node_names.items[index]) == NULL) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered logical node state.");
            goto cleanup;
        }
    }

    for (size_t ln_index = 0U; ln_index < logical_node_names.count; ln_index++) {
        char step_label[160U];
        UnitLabNativeIdentifierList ln_data_names = {0};
        UnitLabNativeIdentifierList ln_brcb_names = {0};

        snprintf(step_label, sizeof(step_label), "ln-data-attributes:%s", logical_node_names.items[ln_index]);
        if (!io->get_name_list_step(session, io, step_label, 3U, 1U, domain_id, logical_node_names.items[ln_index], NULL, followup_invoke_id++)) {
            identifier_list_reset(&ln_data_names);
            identifier_list_reset(&ln_brcb_names);
            goto cleanup;
        }
        if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &ln_data_names, &more_follows, last_identifier, sizeof(last_identifier))) {
            identifier_list_reset(&ln_data_names);
            identifier_list_reset(&ln_brcb_names);
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode LN data GetNameList response.");
            goto cleanup;
        }
        while (more_follows && last_identifier[0] != '\0') {
            size_t before_count = ln_data_names.count;
            snprintf(step_label, sizeof(step_label), "ln-data-attributes-page:%s", logical_node_names.items[ln_index]);
            if (!io->get_name_list_step(session, io, step_label, 3U, 1U, domain_id, logical_node_names.items[ln_index], last_identifier, followup_invoke_id++)) {
                identifier_list_reset(&ln_data_names);
                identifier_list_reset(&ln_brcb_names);
                goto cleanup;
            }
            if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &ln_data_names, &more_follows, last_identifier, sizeof(last_identifier))) {
                identifier_list_reset(&ln_data_names);
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode LN data GetNameList page.");
                goto cleanup;
            }
            if (ln_data_names.count == before_count) {
                identifier_list_reset(&ln_data_names);
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client LN data pagination did not advance.");
                goto cleanup;
            }
        }
        for (size_t data_index = 0U; data_index < ln_data_names.count; data_index++) {
            char data_item[320U];
            UnitLabNativeDiscoveredDataName* data_name = unitlab_native_client_session_append_data_name(session, domain_id, logical_node_names.items[ln_index], ln_data_names.items[data_index]);
            if (data_name == NULL) {
                identifier_list_reset(&ln_data_names);
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered LN data state.");
                goto cleanup;
            }
            snprintf(data_item, sizeof(data_item), "%s$%s", logical_node_names.items[ln_index], ln_data_names.items[data_index]);
            if (!io->attributes_step(session, io, "ln-data-components", domain_id, data_item, followup_invoke_id++, 0)) {
                identifier_list_reset(&ln_data_names);
                identifier_list_reset(&ln_brcb_names);
                goto cleanup;
            }
            if (!collect_get_variable_access_attributes_components_from_frame(session, io, data_name, data_item, io->response, *io->encoded_response_length)) {
                identifier_list_reset(&ln_data_names);
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode LN data component attributes.");
                goto cleanup;
            }
        }
        identifier_list_reset(&ln_data_names);
        snprintf(step_label, sizeof(step_label), "ln-brcbs:%s", logical_node_names.items[ln_index]);
        if (!io->get_name_list_step(session, io, step_label, 4U, 1U, domain_id, logical_node_names.items[ln_index], NULL, followup_invoke_id++)) {
            identifier_list_reset(&ln_brcb_names);
            goto cleanup;
        }
        if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &ln_brcb_names, &more_follows, last_identifier, sizeof(last_identifier))) {
            identifier_list_reset(&ln_brcb_names);
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode BRCB GetNameList response.");
            goto cleanup;
        }
        while (more_follows && last_identifier[0] != '\0') {
            size_t before_count = ln_brcb_names.count;
            snprintf(step_label, sizeof(step_label), "ln-brcbs-page:%s", logical_node_names.items[ln_index]);
            if (!io->get_name_list_step(session, io, step_label, 4U, 1U, domain_id, logical_node_names.items[ln_index], last_identifier, followup_invoke_id++)) {
                identifier_list_reset(&ln_brcb_names);
                goto cleanup;
            }
            if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &ln_brcb_names, &more_follows, last_identifier, sizeof(last_identifier))) {
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode BRCB GetNameList page.");
                goto cleanup;
            }
            if (ln_brcb_names.count == before_count) {
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client BRCB pagination did not advance.");
                goto cleanup;
            }
        }
        for (size_t brcb_index = 0U; brcb_index < ln_brcb_names.count; brcb_index++) {
            if (!identifier_list_append(&brcb_names, ln_brcb_names.items[brcb_index]) || !identifier_list_append(&brcb_logical_nodes, logical_node_names.items[ln_index])) {
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered BRCB state.");
                goto cleanup;
            }
        }
        identifier_list_reset(&ln_brcb_names);

        snprintf(step_label, sizeof(step_label), "ln-urcbs:%s", logical_node_names.items[ln_index]);
        if (!io->get_name_list_step(session, io, step_label, 5U, 1U, domain_id, logical_node_names.items[ln_index], NULL, followup_invoke_id++)) {
            identifier_list_reset(&ln_brcb_names);
            goto cleanup;
        }
        if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &ln_brcb_names, &more_follows, last_identifier, sizeof(last_identifier))) {
            identifier_list_reset(&ln_brcb_names);
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode URCB GetNameList response.");
            goto cleanup;
        }
        while (more_follows && last_identifier[0] != '\0') {
            size_t before_count = ln_brcb_names.count;
            snprintf(step_label, sizeof(step_label), "ln-urcbs-page:%s", logical_node_names.items[ln_index]);
            if (!io->get_name_list_step(session, io, step_label, 5U, 1U, domain_id, logical_node_names.items[ln_index], last_identifier, followup_invoke_id++)) {
                identifier_list_reset(&ln_brcb_names);
                goto cleanup;
            }
            if (!extract_get_name_list_identifiers(io->response, *io->encoded_response_length, &ln_brcb_names, &more_follows, last_identifier, sizeof(last_identifier))) {
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client could not decode URCB GetNameList page.");
                goto cleanup;
            }
            if (ln_brcb_names.count == before_count) {
                identifier_list_reset(&ln_brcb_names);
                set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "Native wire client URCB pagination did not advance.");
                goto cleanup;
            }
        }
        identifier_list_reset(&ln_brcb_names);
    }
    session->discovered_model.brcb_count = brcb_names.count;
    if (brcb_names.count == 0U) {
        printf("native-wire-client: discover-skip=brcb-attrs reason=no-brcb\n");
        fflush(stdout);
    }
    for (size_t index = 0U; index < brcb_names.count; index++) {
        char brcb_item[320U];
        char brcb_read_item[320U];
        snprintf(brcb_item, sizeof(brcb_item), "%s$BR$%s$RptEna", brcb_logical_nodes.items[index], brcb_names.items[index]);
        if (!io->attributes_step(session, io, "brcb-attrs", domain_id, brcb_item, followup_invoke_id++, 0)) {
            goto cleanup;
        }
        snprintf(brcb_read_item, sizeof(brcb_read_item), "%s$BR$%s", brcb_logical_nodes.items[index], brcb_names.items[index]);
        if (unitlab_native_client_session_append_discovered_rcb(session, domain_id, brcb_read_item) == NULL) {
            set_discovery_diagnostic(io, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Native wire client could not allocate discovered BRCB state.");
            goto cleanup;
        }
        printf("native-wire-client: discovered-brcb[%zu] domain=%s item=%s\n", session->discovered_rcb_count - 1U, domain_id, brcb_read_item);
        fflush(stdout);
        if (!io->read_step(session, io, "brcb-values", domain_id, brcb_read_item, followup_invoke_id++)) {
            goto cleanup;
        }
    }
    if (data_set_items.count == 0U) {
        printf("native-wire-client: discover-skip=dataset-members reason=no-dataset\n");
        fflush(stdout);
    }
    for (size_t index = 0U; index < data_set_items.count; index++) {
        char data_set_reference[384U];
        snprintf(data_set_reference, sizeof(data_set_reference), "%s/%s", domain_id, data_set_items.items[index]);
        if (!io->attributes_step(session, io, "dataset-members", domain_id, data_set_items.items[index], followup_invoke_id++, 1)) {
            goto cleanup;
        }
        if (!collect_get_named_variable_list_members_from_frame(session, io, data_set_reference, io->response, *io->encoded_response_length)) {
            goto cleanup;
        }
    }
    *next_invoke_id = followup_invoke_id;
    if (io->emit_model_summary != NULL) {
        io->emit_model_summary(session, "discover");
    }
    ok = 1;

cleanup:
    identifier_list_reset(&logical_device_names);
    identifier_list_reset(&logical_node_names);
    identifier_list_reset(&data_set_items);
    identifier_list_reset(&brcb_names);
    identifier_list_reset(&brcb_logical_nodes);
    return ok;
}
