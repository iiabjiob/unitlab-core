#include "server/unitlab_mms_server_runtime_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Browse service handles discovery/GVA/GNVLA. Current tree helpers preserve native fixture/model-plan compatibility. */

int server_runtime_build_get_name_list_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char** names = NULL;
    size_t name_count = 0U;
    uint8_t visible_strings_bytes[2048U];
    uint8_t list_of_identifier_bytes[2048U];
    uint8_t get_name_list_body_bytes[2048U];
    uint8_t service_payload_bytes[2048U];
    uint8_t service_bytes[2048U];
    size_t visible_strings_length = 0U;
    size_t list_of_identifier_length = 0U;
    size_t get_name_list_body_length = 0U;
    size_t service_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNameList response buffer and encoded_length are required.");
        return 0;
    }
    if (!unitlab_mms_pending_request_collect_get_name_list_names(&server_runtime->pending_request, server_runtime->model_plan, &names, &name_count, diagnostic)) {
        return 0;
    }
    const char* browse_semantics = "GetNameList(UNSUPPORTED)";

    if (server_runtime->pending_request.browse_object_class == 9U && server_runtime->pending_request.browse_object_scope == 0U) {
        browse_semantics = "GetNameList(VMD-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 0U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(DOMAIN-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 2U && server_runtime->pending_request.browse_object_scope == 0U) {
        browse_semantics = "GetNameList(VMD-NVL)";
    } else if (server_runtime->pending_request.browse_object_class == 2U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(DATASET-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 2U && server_runtime->pending_request.browse_object_scope == 2U) {
        browse_semantics = "GetNameList(AA-SPECIFIC)";
    } else if ((server_runtime->pending_request.browse_object_class == 1U || server_runtime->pending_request.browse_object_class == 3U) && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(LOGICAL-NODE-SPECIFIC)";
    } else if (server_runtime->pending_request.browse_object_class == 4U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(BUFFERED-REPORT)";
    } else if (server_runtime->pending_request.browse_object_class == 5U && server_runtime->pending_request.browse_object_scope == 1U) {
        browse_semantics = "GetNameList(UNBUFFERED-REPORT)";
    }

    printf(
        "native-wire-server: confirmed-response invoke=%u service=GetNameList semantic=%s browse-class=%u browse-scope=%u domain=%s continue-after=%s identifiers=%zu moreFollows=false\n",
        (unsigned)invoke_id,
        browse_semantics,
        (unsigned)server_runtime->pending_request.browse_object_class,
        (unsigned)server_runtime->pending_request.browse_object_scope,
        server_runtime->pending_request.browse_domain_id[0] != '\0' ? server_runtime->pending_request.browse_domain_id : "<none>",
        server_runtime->pending_request.browse_continue_after[0] != '\0' ? server_runtime->pending_request.browse_continue_after : "<none>",
        name_count);
    fflush(stdout);
    for (size_t index = 0U; index < name_count; index++) {
        size_t encoded_name_length = 0U;

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
                0,
                26U,
                (const uint8_t*)names[index],
                strlen(names[index]),
                &visible_strings_bytes[visible_strings_length],
                sizeof(visible_strings_bytes) - visible_strings_length,
                &encoded_name_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
        visible_strings_length += encoded_name_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            visible_strings_bytes,
            visible_strings_length,
            list_of_identifier_bytes,
            sizeof(list_of_identifier_bytes),
            &list_of_identifier_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    memcpy(get_name_list_body_bytes, list_of_identifier_bytes, list_of_identifier_length);
    {
        uint8_t more_follows_bytes[1U] = { 0x00U };
        size_t more_follows_length = 0U;

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                1U,
                more_follows_bytes,
                sizeof(more_follows_bytes),
                &get_name_list_body_bytes[list_of_identifier_length],
                sizeof(get_name_list_body_bytes) - list_of_identifier_length,
                &more_follows_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
        get_name_list_body_length = list_of_identifier_length + more_follows_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            get_name_list_body_bytes,
            get_name_list_body_length,
            service_payload_bytes,
            sizeof(service_payload_bytes),
            &service_payload_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            service_bytes,
            sizeof(service_bytes),
            &invoke_id_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (invoke_id_length + service_payload_length > sizeof(service_bytes)) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList response output buffer is too small.");
        return 0;
    }
    memcpy(&service_bytes[invoke_id_length], service_payload_bytes, service_payload_length);
    total_length = invoke_id_length + service_payload_length;
    if (total_length > buffer_length) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNameList response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_store_name_list_summary(server_runtime, invoke_id, "GetNameList", "identifiers", (const char* const*)names, name_count);
    unitlab_free_ied_model_name_list(names, name_count);
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}


static const char* const lln0_mod_children[] = { "q", "t" };
static const char* const lln0_beh_children[] = { "stVal", "q", "t" };
static const char* const lln0_health_children[] = { "stVal", "q", "t" };
static const char* const lln0_cf_children[] = { "Mod" };
static const char* const lln0_cf_mod_children[] = { "ctlModel" };
static const char* const lln0_dc_children[] = { "NamPlt" };
static const char* const lln0_gva_children[] = { "BR" };
static const char* const lln0_br_children[] = { "brcbEvents" };
static const char* const lln0_br_rcb_children[] = {
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
    "TimeOfEntry",
    "ResvTms"
};
static const char* const lln0_ex_children[] = { "NamPlt" };
static const char* const lln0_ex_namplt_children[] = { "ldNs", "lnNs", "cdcNs", "dataNs" };
static const char* const lln0_namplt_children[] = { "vendor", "swRev", "d", "configRev" };
static const char* const xcbr1_gva_children[] = { "ST" };
static const char* const xcbr1_st_children[] = { "Pos" };
static const char* const xcbr1_st_pos_children[] = { "stVal" };
static const char* const pggio1_gva_children[] = { "ST" };
static const char* const pggio1_st_children[] = { "Ind1" };
static const char* const pggio1_st_ind1_children[] = { "stVal" };
static const char* const ggio1_gva_children[] = { "MX" };
static const char* const ggio1_mx_children[] = { "AnIn1" };
static const char* const ggio1_mx_anin1_children[] = { "mag" };
static const char* const ggio1_mx_anin1_mag_children[] = { "f" };

static int server_runtime_encode_gva_leaf_type_spec(
    const char* component_name,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement type_element;
    const uint8_t* value_bytes = NULL;
    size_t value_length = 0U;
    uint8_t value_q_or_bool[1U];
    uint8_t value_visible_2[2U];
    uint8_t value_single[1U];
    uint8_t tag_number = 3U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (component_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA leaf type encoding requires a component name, buffer, and encoded_length.");
        return 0;
    }

    if (strcmp(component_name, "q") == 0) {
        value_q_or_bool[0] = 0xF3U;
        tag_number = 4U;
        value_bytes = value_q_or_bool;
        value_length = sizeof(value_q_or_bool);
    }
    else if (strcmp(component_name, "t") == 0) {
        tag_number = 17U;
        value_bytes = NULL;
        value_length = 0U;
    }
    else if (strcmp(component_name, "stVal") == 0 || strcmp(component_name, "f") == 0) {
        value_single[0] = 0x20U;
        tag_number = 5U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "ctlModel") == 0) {
        value_single[0] = 0x08U;
        tag_number = 5U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "d") == 0 || strcmp(component_name, "vendor") == 0 || strcmp(component_name, "swRev") == 0 || strcmp(component_name, "configRev") == 0) {
        value_visible_2[0] = 0xFFU;
        value_visible_2[1] = 0x01U;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }
    else if (strcmp(component_name, "ldNs") == 0 || strcmp(component_name, "lnNs") == 0 || strcmp(component_name, "cdcNs") == 0 || strcmp(component_name, "dataNs") == 0) {
        value_visible_2[0] = 0x4EU;
        value_visible_2[1] = 0x53U;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }
    else if (strcmp(component_name, "RptID") == 0 || strcmp(component_name, "DatSet") == 0) {
        value_visible_2[0] = 0xFFU;
        value_visible_2[1] = 0x7FU;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }
    else if (strcmp(component_name, "ConfRev") == 0 || strcmp(component_name, "BufTm") == 0 || strcmp(component_name, "IntgPd") == 0) {
        value_single[0] = 0x20U;
        tag_number = 6U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "SqNum") == 0) {
        value_single[0] = 0x10U;
        tag_number = 6U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "ResvTms") == 0) {
        value_single[0] = 0x10U;
        tag_number = 5U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "OptFlds") == 0) {
        value_single[0] = 0xF6U;
        tag_number = 4U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "TrgOps") == 0) {
        value_single[0] = 0xFAU;
        tag_number = 4U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "GI") == 0 || strcmp(component_name, "RptEna") == 0 || strcmp(component_name, "PurgeBuf") == 0) {
        tag_number = 3U;
        value_bytes = NULL;
        value_length = 0U;
    }
    else if (strcmp(component_name, "EntryID") == 0) {
        value_single[0] = 0x08U;
        tag_number = 9U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "TimeOfEntry") == 0 || strcmp(component_name, "TimeofEntry") == 0) {
        value_single[0] = 0x01U;
        tag_number = 12U;
        value_bytes = value_single;
        value_length = sizeof(value_single);
    }
    else if (strcmp(component_name, "Owner") == 0) {
        value_visible_2[0] = 0x4FU;
        value_visible_2[1] = 0x57U;
        tag_number = 10U;
        value_bytes = value_visible_2;
        value_length = sizeof(value_visible_2);
    }

    unitlab_mms_ber_element_init(&type_element);
    type_element.tag.tag_class = UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC;
    type_element.tag.constructed = 0;
    type_element.tag.tag_number = tag_number;
    type_element.value_bytes = value_bytes;
    type_element.value_length = value_length;
    if (!unitlab_mms_ber_write(&type_element, buffer, buffer_length, encoded_length, diagnostic)) {
        return 0;
    }
    return 1;
}

static const char* const* server_runtime_lookup_gva_children(
    const char* logical_node_name,
    const char* parent_component_name,
    const char* component_name,
    size_t* child_count)
{
    if (child_count != NULL) {
        *child_count = 0U;
    }
    if (logical_node_name == NULL || component_name == NULL || child_count == NULL) {
        return NULL;
    }

    if (strcmp(logical_node_name, "XCBR1") == 0) {
        if (parent_component_name == NULL && strcmp(component_name, "ST") == 0) {
            *child_count = sizeof(xcbr1_st_children) / sizeof(xcbr1_st_children[0]);
            return xcbr1_st_children;
        }
        if (parent_component_name != NULL && strcmp(parent_component_name, "ST") == 0 && strcmp(component_name, "Pos") == 0) {
            *child_count = sizeof(xcbr1_st_pos_children) / sizeof(xcbr1_st_pos_children[0]);
            return xcbr1_st_pos_children;
        }
        return NULL;
    }
    if (strcmp(logical_node_name, "PGGIO1") == 0) {
        if (parent_component_name == NULL && strcmp(component_name, "ST") == 0) {
            *child_count = sizeof(pggio1_st_children) / sizeof(pggio1_st_children[0]);
            return pggio1_st_children;
        }
        if (parent_component_name != NULL && strcmp(parent_component_name, "ST") == 0 && strcmp(component_name, "Ind1") == 0) {
            *child_count = sizeof(pggio1_st_ind1_children) / sizeof(pggio1_st_ind1_children[0]);
            return pggio1_st_ind1_children;
        }
        return NULL;
    }
    if (strcmp(logical_node_name, "GGIO1") == 0) {
        if (parent_component_name == NULL && strcmp(component_name, "MX") == 0) {
            *child_count = sizeof(ggio1_mx_children) / sizeof(ggio1_mx_children[0]);
            return ggio1_mx_children;
        }
        if (parent_component_name != NULL && strcmp(parent_component_name, "MX") == 0 && strcmp(component_name, "AnIn1") == 0) {
            *child_count = sizeof(ggio1_mx_anin1_children) / sizeof(ggio1_mx_anin1_children[0]);
            return ggio1_mx_anin1_children;
        }
        if (parent_component_name != NULL && strcmp(parent_component_name, "AnIn1") == 0 && strcmp(component_name, "mag") == 0) {
            *child_count = sizeof(ggio1_mx_anin1_mag_children) / sizeof(ggio1_mx_anin1_mag_children[0]);
            return ggio1_mx_anin1_mag_children;
        }
        return NULL;
    }
    if (strcmp(logical_node_name, "LLN0") != 0) {
        return NULL;
    }
    if (parent_component_name == NULL) {
        if (strcmp(component_name, "Mod") == 0) {
            *child_count = sizeof(lln0_mod_children) / sizeof(lln0_mod_children[0]);
            return lln0_mod_children;
        }
        if (strcmp(component_name, "Beh") == 0) {
            *child_count = sizeof(lln0_beh_children) / sizeof(lln0_beh_children[0]);
            return lln0_beh_children;
        }
        if (strcmp(component_name, "Health") == 0) {
            *child_count = sizeof(lln0_health_children) / sizeof(lln0_health_children[0]);
            return lln0_health_children;
        }
        if (strcmp(component_name, "CF") == 0) {
            *child_count = sizeof(lln0_cf_children) / sizeof(lln0_cf_children[0]);
            return lln0_cf_children;
        }
        if (strcmp(component_name, "DC") == 0) {
            *child_count = sizeof(lln0_dc_children) / sizeof(lln0_dc_children[0]);
            return lln0_dc_children;
        }
        if (strcmp(component_name, "NamPlt") == 0) {
            *child_count = sizeof(lln0_namplt_children) / sizeof(lln0_namplt_children[0]);
            return lln0_namplt_children;
        }
        if (strcmp(component_name, "EX") == 0) {
            *child_count = sizeof(lln0_ex_children) / sizeof(lln0_ex_children[0]);
            return lln0_ex_children;
        }
        if (strcmp(component_name, "BR") == 0) {
            *child_count = sizeof(lln0_br_children) / sizeof(lln0_br_children[0]);
            return lln0_br_children;
        }
    }
    else if (strcmp(parent_component_name, "CF") == 0 && strcmp(component_name, "Mod") == 0) {
        *child_count = sizeof(lln0_cf_mod_children) / sizeof(lln0_cf_mod_children[0]);
        return lln0_cf_mod_children;
    }
    else if (strcmp(parent_component_name, "DC") == 0 && strcmp(component_name, "NamPlt") == 0) {
        *child_count = sizeof(lln0_namplt_children) / sizeof(lln0_namplt_children[0]);
        return lln0_namplt_children;
    }
    else if (strcmp(parent_component_name, "BR") == 0 && strcmp(component_name, "brcbEvents") == 0) {
        *child_count = sizeof(lln0_br_rcb_children) / sizeof(lln0_br_rcb_children[0]);
        return lln0_br_rcb_children;
    }
    else if (strcmp(parent_component_name, "EX") == 0 && strcmp(component_name, "NamPlt") == 0) {
        *child_count = sizeof(lln0_ex_namplt_children) / sizeof(lln0_ex_namplt_children[0]);
        return lln0_ex_namplt_children;
    }
    return NULL;
}


static int server_runtime_copy_static_names(
    const char* const* source_names,
    size_t source_count,
    char*** names,
    size_t* count,
    UnitLabMmsDiagnostic* diagnostic)
{
    if (names == NULL || count == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Static GVA name copy requires output pointers.");
        return 0;
    }
    *names = NULL;
    *count = 0U;
    if (source_count == 0U) {
        return 1;
    }
    *names = (char**)calloc(source_count, sizeof(char*));
    if (*names == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Cannot allocate static GVA names.");
        return 0;
    }
    for (size_t index = 0U; index < source_count; index++) {
        size_t length = strlen(source_names[index]);

        (*names)[index] = (char*)calloc(length + 1U, sizeof(char));
        if ((*names)[index] == NULL) {
            unitlab_free_ied_model_name_list(*names, index);
            *names = NULL;
            *count = 0U;
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Cannot allocate static GVA name.");
            return 0;
        }
        memcpy((*names)[index], source_names[index], length + 1U);
        *count = index + 1U;
    }
    return 1;
}

static int server_runtime_encode_gva_component_tree(
    const char* logical_node_name,
    const char* parent_component_name,
    const char* component_name,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t component_name_bytes[256U];
    uint8_t component_type_bytes[8192U];
    uint8_t component_components_wrapper_bytes[12288U];
    uint8_t component_structure_bytes[16384U];
    uint8_t component_type_wrapper_bytes[20480U];
    uint8_t component_content_bytes[16384U];
    uint8_t component_bytes[32768U];
    const char* const* child_names = NULL;
    size_t child_count = 0U;
    size_t component_name_length = 0U;
    size_t component_type_length = 0U;
    size_t component_components_wrapper_length = 0U;
    size_t component_structure_length = 0U;
    size_t component_type_wrapper_length = 0U;
    size_t component_content_length = 0U;
    size_t component_length = 0U;
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (component_name == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA component encoding requires a component name, buffer, and encoded_length.");
        return 0;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            0,
            0U,
            (const uint8_t*)component_name,
            strlen(component_name),
            component_name_bytes,
            sizeof(component_name_bytes),
            &component_name_length,
            diagnostic)) {
        return 0;
    }

    child_names = server_runtime_lookup_gva_children(logical_node_name, parent_component_name, component_name, &child_count);
    if (child_names != NULL && child_count > 0U) {
        size_t child_component_bytes_length = 0U;

        for (size_t child_index = 0U; child_index < child_count; child_index++) {
            size_t child_length = 0U;

            if (!server_runtime_encode_gva_component_tree(
                    logical_node_name,
                    component_name,
                    child_names[child_index],
                    &component_content_bytes[child_component_bytes_length],
                    sizeof(component_content_bytes) - child_component_bytes_length,
                    &child_length,
                    diagnostic)) {
                return 0;
            }
            child_component_bytes_length += child_length;
        }
        if (child_component_bytes_length > sizeof(component_type_bytes)) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA component type buffer is too small.");
            return 0;
        }
        memcpy(component_type_bytes, component_content_bytes, child_component_bytes_length);
        component_type_length = child_component_bytes_length;
        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                1,
                1U,
                component_type_bytes,
                component_type_length,
                component_components_wrapper_bytes,
                sizeof(component_components_wrapper_bytes),
                &component_components_wrapper_length,
                diagnostic)) {
            return 0;
        }
        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                1,
                2U,
                component_components_wrapper_bytes,
                component_components_wrapper_length,
                component_structure_bytes,
                sizeof(component_structure_bytes),
                &component_structure_length,
                diagnostic)) {
            return 0;
        }
        memcpy(component_type_bytes, component_structure_bytes, component_structure_length);
        component_type_length = component_structure_length;
    } else {
        if (!server_runtime_encode_gva_leaf_type_spec(
                component_name,
                component_type_bytes,
                sizeof(component_type_bytes),
                &component_type_length,
                diagnostic)) {
            return 0;
        }
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            component_type_bytes,
            component_type_length,
            component_type_wrapper_bytes,
            sizeof(component_type_wrapper_bytes),
            &component_type_wrapper_length,
            diagnostic)) {
        return 0;
    }
    if (component_name_length + component_type_wrapper_length > sizeof(component_content_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA component encoding buffer is too small.");
        return 0;
    }
    memcpy(component_content_bytes, component_name_bytes, component_name_length);
    memcpy(&component_content_bytes[component_name_length], component_type_wrapper_bytes, component_type_wrapper_length);
    component_content_length = component_name_length + component_type_wrapper_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            component_content_bytes,
            component_content_length,
            component_bytes,
            sizeof(component_bytes),
            &component_length,
            diagnostic)) {
        return 0;
    }
    if (component_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA component output buffer is too small.");
        return 0;
    }
    memcpy(buffer, component_bytes, component_length);
    *encoded_length = component_length;
    return 1;
}

int server_runtime_build_get_variable_access_attributes_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    const char* object_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char domain_id[128U];
    char item_id[128U];
    char model_error[256U];
    char logical_node_for_gva[128U];
    const char* root_parent_component_name = NULL;
    char** names = NULL;
    size_t name_count = 0U;
    uint8_t component_bytes[4096U];
    uint8_t components_wrapper_bytes[4096U];
    uint8_t type_spec_bytes[4096U];
    uint8_t type_spec_wrapper_bytes[4096U];
    uint8_t response_payload_bytes[8192U];
    uint8_t service_payload_bytes[8192U];
    uint8_t service_bytes[8192U];
    size_t component_bytes_length = 0U;
    size_t components_wrapper_length = 0U;
    size_t type_spec_length = 0U;
    size_t type_spec_wrapper_length = 0U;
    size_t response_payload_length = 0U;
    size_t service_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA response buffer and encoded_length are required.");
        return 0;
    }
    if (object_reference == NULL || object_reference[0] == '\0') {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GVA response requires an object reference.");
        return 0;
    }
    if (!server_runtime_parse_object_reference(object_reference, domain_id, sizeof(domain_id), item_id, sizeof(item_id))) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_PROTOCOL_ERROR, "GVA response object reference is malformed.");
        return 0;
    }

    model_error[0] = '\0';
    snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", item_id);
    if (strcmp(item_id, "LLN0") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "LLN0");
        if (!server_runtime_copy_static_names(lln0_gva_children, sizeof(lln0_gva_children) / sizeof(lln0_gva_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (strcmp(item_id, "LLN0$BR") == 0 || strcmp(item_id, "LLN0.BR") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "LLN0");
        root_parent_component_name = "BR";
        if (!server_runtime_copy_static_names(lln0_br_children, sizeof(lln0_br_children) / sizeof(lln0_br_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (strcmp(item_id, "LLN0$BR$brcbEvents") == 0 || strcmp(item_id, "LLN0.BR.brcbEvents") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "LLN0");
        if (!server_runtime_copy_static_names(lln0_br_rcb_children, sizeof(lln0_br_rcb_children) / sizeof(lln0_br_rcb_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (strcmp(item_id, "XCBR1") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "XCBR1");
        if (!server_runtime_copy_static_names(xcbr1_gva_children, sizeof(xcbr1_gva_children) / sizeof(xcbr1_gva_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (strcmp(item_id, "PGGIO1") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "PGGIO1");
        if (!server_runtime_copy_static_names(pggio1_gva_children, sizeof(pggio1_gva_children) / sizeof(pggio1_gva_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (strcmp(item_id, "GGIO1") == 0) {
        snprintf(logical_node_for_gva, sizeof(logical_node_for_gva), "%s", "GGIO1");
        if (!server_runtime_copy_static_names(ggio1_gva_children, sizeof(ggio1_gva_children) / sizeof(ggio1_gva_children[0]), &names, &name_count, diagnostic)) {
            return 0;
        }
    }
    else if (domain_id[0] != '\0') {
        if (!unitlab_collect_ied_model_logical_node_variables(
                server_runtime->model_plan,
                domain_id,
                item_id,
                &names,
                &name_count,
                model_error,
                sizeof(model_error))) {
            unitlab_free_ied_model_name_list(names, name_count);
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GVA response lookup failed.");
            return 0;
        }
    } else {
        if (!unitlab_collect_ied_model_vmd_named_variable_lists(
                server_runtime->model_plan,
                &names,
                &name_count,
                model_error,
                sizeof(model_error))) {
            unitlab_free_ied_model_name_list(names, name_count);
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, model_error[0] != '\0' ? model_error : "GVA response lookup failed.");
            return 0;
        }
    }

    printf(
        "native-wire-server: confirmed-response invoke=%u service=GetVariableAccessAttributes object=%s attribute=%s type-kind=structure components=%zu\n",
        (unsigned)invoke_id,
        object_reference,
        item_id,
        name_count);
    for (size_t index = 0U; index < name_count; index++) {
        printf("native-wire-server: gva-component[%zu]=%s\n", index, names[index]);
    }
    fflush(stdout);
    for (size_t index = 0U; index < name_count; index++) {
        size_t component_length = 0U;

        if (!server_runtime_encode_gva_component_tree(
                logical_node_for_gva,
                root_parent_component_name,
                names[index],
                &component_bytes[component_bytes_length],
                sizeof(component_bytes) - component_bytes_length,
                &component_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
        component_bytes_length += component_length;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            component_bytes,
            component_bytes_length,
            components_wrapper_bytes,
            sizeof(components_wrapper_bytes),
            &components_wrapper_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            components_wrapper_bytes,
            components_wrapper_length,
            type_spec_bytes,
            sizeof(type_spec_bytes),
            &type_spec_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }

    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            2U,
            type_spec_bytes,
            type_spec_length,
            type_spec_wrapper_bytes,
            sizeof(type_spec_wrapper_bytes),
            &type_spec_wrapper_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }

    {
        uint8_t false_byte[1U] = { 0x00U };

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                0U,
                false_byte,
                sizeof(false_byte),
                response_payload_bytes,
                sizeof(response_payload_bytes),
                &response_payload_length,
                diagnostic)) {
            unitlab_free_ied_model_name_list(names, name_count);
            return 0;
        }
    }
    if (response_payload_length + type_spec_wrapper_length > sizeof(response_payload_bytes)) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA response output buffer is too small.");
        return 0;
    }
    memcpy(&response_payload_bytes[response_payload_length], type_spec_wrapper_bytes, type_spec_wrapper_length);
    response_payload_length += type_spec_wrapper_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            6U,
            response_payload_bytes,
            response_payload_length,
            service_payload_bytes,
            sizeof(service_payload_bytes),
            &service_payload_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            service_bytes,
            sizeof(service_bytes),
            &invoke_id_length,
            diagnostic)) {
        unitlab_free_ied_model_name_list(names, name_count);
        return 0;
    }
    if (invoke_id_length + service_payload_length > sizeof(service_bytes)) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA response output buffer is too small.");
        return 0;
    }
    memcpy(&service_bytes[invoke_id_length], service_payload_bytes, service_payload_length);
    total_length = invoke_id_length + service_payload_length;
    if (total_length > buffer_length) {
        unitlab_free_ied_model_name_list(names, name_count);
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GVA response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_store_name_list_summary(server_runtime, invoke_id, "GetVariableAccessAttributes", "components", (const char* const*)names, name_count);
    unitlab_free_ied_model_name_list(names, name_count);
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}



static int server_runtime_parse_named_variable_list_reference(
    const char* request_reference,
    char* logical_device_inst,
    size_t logical_device_inst_size,
    char* logical_node_name,
    size_t logical_node_name_size,
    char* list_name,
    size_t list_name_size)
{
    const char* slash = NULL;
    const char* dollar = NULL;
    size_t logical_device_length = 0U;
    size_t logical_node_length = 0U;
    size_t list_name_length = 0U;

    if (logical_device_inst == NULL || logical_device_inst_size == 0U || logical_node_name == NULL || logical_node_name_size == 0U || list_name == NULL || list_name_size == 0U) {
        return 0;
    }
    logical_device_inst[0] = '\0';
    logical_node_name[0] = '\0';
    list_name[0] = '\0';
    if (request_reference == NULL || request_reference[0] == '\0') {
        return 0;
    }

    slash = strchr(request_reference, '/');
    if (slash == NULL) {
        list_name_length = strlen(request_reference);
        if (list_name_length == 0U || list_name_length >= list_name_size) {
            return 0;
        }
        memcpy(list_name, request_reference, list_name_length + 1U);
        return 1;
    }

    logical_device_length = (size_t)(slash - request_reference);
    if (logical_device_length == 0U || logical_device_length >= logical_device_inst_size) {
        return 0;
    }
    memcpy(logical_device_inst, request_reference, logical_device_length);
    logical_device_inst[logical_device_length] = '\0';

    dollar = strchr(slash + 1, '$');
    if (dollar == NULL || dollar == slash + 1) {
        return 0;
    }
    logical_node_length = (size_t)(dollar - (slash + 1));
    if (logical_node_length == 0U || logical_node_length >= logical_node_name_size) {
        return 0;
    }
    memcpy(logical_node_name, slash + 1, logical_node_length);
    logical_node_name[logical_node_length] = '\0';

    list_name_length = strlen(dollar + 1);
    if (list_name_length == 0U || list_name_length >= list_name_size) {
        return 0;
    }
    memcpy(list_name, dollar + 1, list_name_length + 1U);
    return 1;
}

static const UnitLabIedModelDataSet* server_runtime_find_named_variable_list_data_set(
    const UnitLabMmsServerRuntime* server_runtime,
    const char* request_reference,
    char* logical_device_inst,
    size_t logical_device_inst_size,
    char* logical_node_name,
    size_t logical_node_name_size,
    char* list_name,
    size_t list_name_size)
{
    if (server_runtime == NULL || server_runtime->model_plan == NULL) {
        return NULL;
    }
    if (!server_runtime_parse_named_variable_list_reference(
            request_reference,
            logical_device_inst,
            logical_device_inst_size,
            logical_node_name,
            logical_node_name_size,
            list_name,
            list_name_size)) {
        return NULL;
    }

    if (logical_device_inst[0] == '\0') {
        const UnitLabIedModelDataSet* matched_data_set = NULL;

        for (size_t index = 0U; index < server_runtime->model_plan->data_set_count; index++) {
            const UnitLabIedModelDataSet* data_set = &server_runtime->model_plan->data_sets[index];

            if (strcmp(data_set->name, list_name) != 0) {
                continue;
            }
            if (matched_data_set != NULL) {
                return NULL;
            }
            matched_data_set = data_set;
        }
        if (matched_data_set == NULL) {
            return NULL;
        }
        snprintf(logical_device_inst, logical_device_inst_size, "%s", matched_data_set->logical_device_inst);
        snprintf(logical_node_name, logical_node_name_size, "%s", matched_data_set->logical_node_name);
        return matched_data_set;
    }

    return unitlab_find_ied_model_data_set(server_runtime->model_plan, logical_device_inst, logical_node_name, list_name);
}

static int server_runtime_encode_named_variable_list_object_name(
    const char* domain_id,
    const char* item_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t domain_bytes[256U];
    uint8_t item_bytes[512U];
    uint8_t domainspecific_bytes[1024U];
    uint8_t object_name_bytes[1200U];
    size_t domain_length = 0U;
    size_t item_length = 0U;
    size_t object_name_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (domain_id == NULL || item_id == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list object name encoding requires domain, item, buffer, and encoded_length.");
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            26U,
            (const uint8_t*)domain_id,
            strlen(domain_id),
            domain_bytes,
            sizeof(domain_bytes),
            &domain_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            0,
            26U,
            (const uint8_t*)item_id,
            strlen(item_id),
            item_bytes,
            sizeof(item_bytes),
            &item_length,
            diagnostic)) {
        return 0;
    }
    if (domain_length + item_length > sizeof(domainspecific_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list object name buffer is too small.");
        return 0;
    }
    memcpy(domainspecific_bytes, domain_bytes, domain_length);
    memcpy(&domainspecific_bytes[domain_length], item_bytes, item_length);
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            domainspecific_bytes,
            domain_length + item_length,
            object_name_bytes,
            sizeof(object_name_bytes),
            &object_name_length,
            diagnostic)) {
        return 0;
    }
    if (object_name_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list object name output buffer is too small.");
        return 0;
    }
    memcpy(buffer, object_name_bytes, object_name_length);
    *encoded_length = object_name_length;
    return 1;
}

static int server_runtime_encode_named_variable_list_member_item(
    const UnitLabIedModelSignal* signal,
    char* domain_id,
    size_t domain_id_size,
    char* item_id,
    size_t item_id_size,
    UnitLabMmsDiagnostic* diagnostic)
{
    const char* entry = NULL;
    const char* slash = NULL;
    size_t domain_length = 0U;
    size_t item_length = 0U;
    int written;

    if (domain_id != NULL && domain_id_size > 0U) {
        domain_id[0] = '\0';
    }
    if (item_id != NULL && item_id_size > 0U) {
        item_id[0] = '\0';
    }
    if (signal == NULL || domain_id == NULL || domain_id_size == 0U || item_id == NULL || item_id_size == 0U) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list member item encoding requires a signal, output buffers, and output lengths.");
        return 0;
    }

    entry = signal->data_set_entry_variable;
    if (entry != NULL && entry[0] != '\0') {
        slash = strchr(entry, '/');
        if (slash != NULL) {
            domain_length = (size_t)(slash - entry);
            if (domain_length == 0U || domain_length >= domain_id_size) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member domain output buffer is too small.");
                return 0;
            }
            memcpy(domain_id, entry, domain_length);
            domain_id[domain_length] = '\0';
            entry = slash + 1;
        }
    }
    if (domain_id[0] == '\0' && signal->logical_device_inst[0] != '\0') {
        written = snprintf(domain_id, domain_id_size, "%s", signal->logical_device_inst);
        if (written <= 0 || (size_t)written >= domain_id_size) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member domain output buffer is too small.");
            return 0;
        }
    }
    if (entry == NULL || entry[0] == '\0') {
        if (signal->logical_node_name[0] == '\0' || signal->fc[0] == '\0' || signal->object_reference[0] == '\0') {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list member item encoding requires a dataset entry variable or logical node, functional constraint, and object reference.");
            return 0;
        }

        written = snprintf(item_id, item_id_size, "%s$%s$", signal->logical_node_name, signal->fc);
        if (written <= 0 || (size_t)written >= item_id_size) {
            server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member item output buffer is too small.");
            return 0;
        }
        item_length = (size_t)written;
        for (const char* cursor = signal->object_reference; *cursor != '\0'; cursor++) {
            if (item_length + 1U >= item_id_size) {
                server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member item output buffer is too small.");
                return 0;
            }
            item_id[item_length++] = (*cursor == '.') ? '$' : *cursor;
        }
        item_id[item_length] = '\0';
        return 1;
    }
    written = snprintf(item_id, item_id_size, "%s", entry);
    if (written <= 0 || (size_t)written >= item_id_size) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member item output buffer is too small.");
        return 0;
    }
    return 1;
}

static const char* server_runtime_ber_tag_class_label(UnitLabMmsBerTagClass tag_class)
{
    switch (tag_class) {
        case UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL:
            return "UNIVERSAL";
        case UNITLAB_MMS_BER_TAG_CLASS_APPLICATION:
            return "APPLICATION";
        case UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC:
            return "CONTEXT-SPECIFIC";
        case UNITLAB_MMS_BER_TAG_CLASS_PRIVATE:
            return "PRIVATE";
        default:
            return "UNKNOWN";
    }
}

static void server_runtime_log_ber_element_line(const char* prefix, const UnitLabMmsBerElement* element)
{
    if (prefix == NULL || element == NULL) {
        return;
    }
    printf(
        "%s tag=%s constructed=%u number=%u length=%zu\n",
        prefix,
        server_runtime_ber_tag_class_label(element->tag.tag_class),
        (unsigned)element->tag.constructed,
        (unsigned)element->tag.tag_number,
        element->value_length);
}

static void server_runtime_log_get_named_variable_list_attributes_member_tree(
    size_t member_index,
    const uint8_t* member_bytes,
    size_t member_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement member;
    UnitLabMmsBerElement variable_spec;
    UnitLabMmsBerElement object_name;
    UnitLabMmsBerElement child;
    size_t consumed_length = 0U;
    size_t child_consumed_length = 0U;
    size_t offset = 0U;
    char domain_id[128U];
    char item_id[256U];

    if (member_bytes == NULL || member_length == 0U) {
        return;
    }

    domain_id[0] = '\0';
    item_id[0] = '\0';
    unitlab_mms_ber_element_init(&member);
    if (!unitlab_mms_ber_read(&member, member_bytes, member_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: gnvla-ber member", &member);

    unitlab_mms_ber_element_init(&variable_spec);
    if (!unitlab_mms_ber_read(&variable_spec, member.value_bytes, member.value_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] variable-spec decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber member[%zu] VariableSpecification choice tag=%s constructed=%u number=%u length=%zu\n",
        member_index,
        server_runtime_ber_tag_class_label(variable_spec.tag.tag_class),
        (unsigned)variable_spec.tag.constructed,
        (unsigned)variable_spec.tag.tag_number,
        variable_spec.value_length);

    unitlab_mms_ber_element_init(&object_name);
    if (!unitlab_mms_ber_read(&object_name, variable_spec.value_bytes, variable_spec.value_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] object-name decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber member[%zu] ObjectName choice tag=%s constructed=%u number=%u length=%zu\n",
        member_index,
        server_runtime_ber_tag_class_label(object_name.tag.tag_class),
        (unsigned)object_name.tag.constructed,
        (unsigned)object_name.tag.tag_number,
        object_name.value_length);

    unitlab_mms_ber_element_init(&child);
    if (!unitlab_mms_ber_read(&child, object_name.value_bytes, object_name.value_length, &child_consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber member[%zu] domain-id decode-failed\n", member_index);
        fflush(stdout);
        return;
    }
    if (child.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && child.tag.tag_number == 26U && !child.tag.constructed) {
        size_t copy_length = child.value_length < sizeof(domain_id) - 1U ? child.value_length : sizeof(domain_id) - 1U;
        memcpy(domain_id, child.value_bytes, copy_length);
        domain_id[copy_length] = '\0';
    }
    printf(
        "native-wire-server: gnvla-ber member[%zu] ObjectName.domainId tag=%s constructed=%u number=%u value=\"%s\"\n",
        member_index,
        server_runtime_ber_tag_class_label(child.tag.tag_class),
        (unsigned)child.tag.constructed,
        (unsigned)child.tag.tag_number,
        domain_id);
    offset += child_consumed_length;

    unitlab_mms_ber_element_init(&child);
    if (offset < object_name.value_length && unitlab_mms_ber_read(&child, &object_name.value_bytes[offset], object_name.value_length - offset, &child_consumed_length, diagnostic)) {
        if (child.tag.tag_class == UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL && child.tag.tag_number == 26U && !child.tag.constructed) {
            size_t copy_length = child.value_length < sizeof(item_id) - 1U ? child.value_length : sizeof(item_id) - 1U;
            memcpy(item_id, child.value_bytes, copy_length);
            item_id[copy_length] = '\0';
        }
        printf(
            "native-wire-server: gnvla-ber member[%zu] ObjectName.itemId tag=%s constructed=%u number=%u value=\"%s\"\n",
            member_index,
            server_runtime_ber_tag_class_label(child.tag.tag_class),
            (unsigned)child.tag.constructed,
            (unsigned)child.tag.tag_number,
            item_id);
    }
    fflush(stdout);
}

static void server_runtime_log_get_named_variable_list_attributes_response_tree(
    uint32_t invoke_id,
    const uint8_t* service_bytes,
    size_t service_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    UnitLabMmsBerElement invoke_id_element;
    UnitLabMmsBerElement service_element;
    UnitLabMmsBerElement deletable_element;
    UnitLabMmsBerElement list_of_variable_element;
    UnitLabMmsBerElement member;
    size_t consumed_length = 0U;
    size_t response_consumed_length = 0U;
    size_t member_consumed_length = 0U;
    size_t offset = 0U;
    size_t member_index = 0U;

    if (service_bytes == NULL || service_length == 0U) {
        return;
    }
    unitlab_mms_ber_element_init(&invoke_id_element);
    if (!unitlab_mms_ber_read(&invoke_id_element, service_bytes, service_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-invoke\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: gnvla-ber invoke-id", &invoke_id_element);

    unitlab_mms_ber_element_init(&service_element);
    if (!unitlab_mms_ber_read(&service_element, &service_bytes[consumed_length], service_length - consumed_length, &response_consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-response\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    server_runtime_log_ber_element_line("native-wire-server: gnvla-ber response", &service_element);

    unitlab_mms_ber_element_init(&deletable_element);
    if (!unitlab_mms_ber_read(&deletable_element, service_element.value_bytes, service_element.value_length, &consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-deletable\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber invoke=%u deletable tag=%s constructed=%u number=%u value=%u\n",
        (unsigned)invoke_id,
        server_runtime_ber_tag_class_label(deletable_element.tag.tag_class),
        (unsigned)deletable_element.tag.constructed,
        (unsigned)deletable_element.tag.tag_number,
        deletable_element.value_length > 0U && deletable_element.value_bytes[0] != 0U ? 1U : 0U);

    unitlab_mms_ber_element_init(&list_of_variable_element);
    if (!unitlab_mms_ber_read(&list_of_variable_element, &service_element.value_bytes[consumed_length], service_element.value_length - consumed_length, &response_consumed_length, diagnostic)) {
        printf("native-wire-server: gnvla-ber invoke=%u decode-failed-at-listOfVariable\n", (unsigned)invoke_id);
        fflush(stdout);
        return;
    }
    printf(
        "native-wire-server: gnvla-ber invoke=%u listOfVariable tag=%s constructed=%u number=%u length=%zu\n",
        (unsigned)invoke_id,
        server_runtime_ber_tag_class_label(list_of_variable_element.tag.tag_class),
        (unsigned)list_of_variable_element.tag.constructed,
        (unsigned)list_of_variable_element.tag.tag_number,
        list_of_variable_element.value_length);

    while (offset < list_of_variable_element.value_length) {
        unitlab_mms_ber_element_init(&member);
        if (!unitlab_mms_ber_read(&member, &list_of_variable_element.value_bytes[offset], list_of_variable_element.value_length - offset, &member_consumed_length, diagnostic)) {
            printf("native-wire-server: gnvla-ber invoke=%u member[%zu] decode-failed\n", (unsigned)invoke_id, member_index);
            fflush(stdout);
            return;
        }
        server_runtime_log_get_named_variable_list_attributes_member_tree(
            member_index,
            &list_of_variable_element.value_bytes[offset],
            member_consumed_length,
            diagnostic);
        offset += member_consumed_length;
        member_index++;
    }
}

static int server_runtime_encode_named_variable_list_member(

    const char* domain_id,
    const char* item_id,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    uint8_t object_name_bytes[1300U];
    uint8_t variable_spec_bytes[1600U];
    uint8_t member_bytes[1800U];
    size_t object_name_length = 0U;
    size_t variable_spec_length = 0U;
    size_t member_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (domain_id == NULL || item_id == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "Named variable list member encoding requires domain, item, buffer, and encoded_length.");
        return 0;
    }
    if (!server_runtime_encode_named_variable_list_object_name(
            domain_id,
            item_id,
            object_name_bytes,
            sizeof(object_name_bytes),
            &object_name_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            0U,
            object_name_bytes,
            object_name_length,
            variable_spec_bytes,
            sizeof(variable_spec_bytes),
            &variable_spec_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_UNIVERSAL,
            1,
            16U,
            variable_spec_bytes,
            variable_spec_length,
            member_bytes,
            sizeof(member_bytes),
            &member_length,
            diagnostic)) {
        return 0;
    }
    if (member_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "Named variable list member output buffer is too small.");
        return 0;
    }
    memcpy(buffer, member_bytes, member_length);
    *encoded_length = member_length;
    return 1;
}

int server_runtime_build_get_named_variable_list_attributes_response_service(
    UnitLabMmsServerRuntime* server_runtime,
    uint32_t invoke_id,
    const char* object_reference,
    uint8_t* buffer,
    size_t buffer_length,
    size_t* encoded_length,
    UnitLabMmsDiagnostic* diagnostic)
{
    char logical_device_inst[128U];
    char logical_node_name[128U];
    char list_name[128U];
    const UnitLabIedModelDataSet* data_set = NULL;
    uint8_t member_bytes[60000U];
    uint8_t list_of_variable_bytes[61000U];
    uint8_t response_payload_bytes[62000U];
    uint8_t service_payload_bytes[63000U];
    uint8_t service_bytes[64000U];
    size_t member_bytes_length = 0U;
    size_t list_of_variable_length = 0U;
    size_t response_payload_length = 0U;
    size_t service_payload_length = 0U;
    size_t invoke_id_length = 0U;
    size_t total_length = 0U;

    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }
    if (server_runtime == NULL || buffer == NULL || encoded_length == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNamedVariableListAttributes response buffer and encoded_length are required.");
        return 0;
    }
    if (object_reference == NULL || object_reference[0] == '\0') {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT, "GetNamedVariableListAttributes response requires a list reference.");
        return 0;
    }
    data_set = server_runtime_find_named_variable_list_data_set(
        server_runtime,
        object_reference,
        logical_device_inst,
        sizeof(logical_device_inst),
        logical_node_name,
        sizeof(logical_node_name),
        list_name,
        sizeof(list_name));
    if (data_set == NULL) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_UNSUPPORTED, "GetNamedVariableListAttributes list lookup failed.");
        return 0;
    }

    printf(
        "native-wire-server: confirmed-response invoke=%u service=GetNamedVariableListAttributes list=%s domain=%s node=%s members=%zu deletable=false\n",
        (unsigned)invoke_id,
        list_name,
        logical_device_inst,
        logical_node_name,
        data_set->member_count);
    fflush(stdout);

    for (size_t member_index = 0U; member_index < data_set->member_count; member_index++) {
        const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[data_set->first_signal_index + member_index];
        char member_item[256U];
        size_t member_length = 0U;

        if (!server_runtime_encode_named_variable_list_member_item(
                signal,
                logical_device_inst,
                sizeof(logical_device_inst),
                member_item,
                sizeof(member_item),
                diagnostic)) {
            return 0;
        }
        if (!server_runtime_encode_named_variable_list_member(
                logical_device_inst,
                member_item,
                &member_bytes[member_bytes_length],
                sizeof(member_bytes) - member_bytes_length,
                &member_length,
                diagnostic)) {
            return 0;
        }
        member_bytes_length += member_length;
        printf("native-wire-server: nvl-attribute-member[%zu]=%s/%s\n", member_index, logical_device_inst, member_item);
    }

    {
        uint8_t false_byte[1U] = { 0x00U };
        size_t mms_deletable_length = 0U;

        if (!server_runtime_encode_ber_element(
                UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
                0,
                0U,
                false_byte,
                sizeof(false_byte),
                response_payload_bytes,
                sizeof(response_payload_bytes),
                &mms_deletable_length,
                diagnostic)) {
            return 0;
        }
        response_payload_length = mms_deletable_length;
    }
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            1U,
            member_bytes,
            member_bytes_length,
            list_of_variable_bytes,
            sizeof(list_of_variable_bytes),
            &list_of_variable_length,
            diagnostic)) {
        return 0;
    }
    if (response_payload_length + list_of_variable_length > sizeof(response_payload_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes response output buffer is too small.");
        return 0;
    }
    memcpy(&response_payload_bytes[response_payload_length], list_of_variable_bytes, list_of_variable_length);
    response_payload_length += list_of_variable_length;
    if (!server_runtime_encode_ber_element(
            UNITLAB_MMS_BER_TAG_CLASS_CONTEXT_SPECIFIC,
            1,
            12U,
            response_payload_bytes,
            response_payload_length,
            service_payload_bytes,
            sizeof(service_payload_bytes),
            &service_payload_length,
            diagnostic)) {
        return 0;
    }
    if (!server_runtime_encode_invoke_id_element(
            invoke_id,
            service_bytes,
            sizeof(service_bytes),
            &invoke_id_length,
            diagnostic)) {
        return 0;
    }
    if (invoke_id_length + service_payload_length > sizeof(service_bytes)) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes response output buffer is too small.");
        return 0;
    }
    memcpy(&service_bytes[invoke_id_length], service_payload_bytes, service_payload_length);
    total_length = invoke_id_length + service_payload_length;
    if (total_length > buffer_length) {
        server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_BUFFER_TOO_SMALL, "GetNamedVariableListAttributes response output buffer is too small.");
        return 0;
    }
    memcpy(buffer, service_bytes, total_length);
    *encoded_length = total_length;
    server_runtime_log_get_named_variable_list_attributes_response_tree(invoke_id, service_bytes, total_length, diagnostic);
    {
        char summary[512U];
        size_t offset = 0U;

        offset = (size_t)snprintf(summary, sizeof(summary), "variables=%zu list=[", data_set->member_count);
        for (size_t member_index = 0U; member_index < data_set->member_count && offset < sizeof(summary); member_index++) {
            const UnitLabIedModelSignal* signal = &server_runtime->model_plan->signals[data_set->first_signal_index + member_index];
            int written = snprintf(&summary[offset], sizeof(summary) - offset, "%s%s", member_index > 0U ? "," : "", signal->data_set_entry_variable);
            if (written < 0) {
                summary[0] = '\0';
                break;
            }
            if ((size_t)written >= sizeof(summary) - offset) {
                offset = sizeof(summary) - 1U;
                break;
            }
            offset += (size_t)written;
        }
        if (offset < sizeof(summary) - 1U) {
            snprintf(&summary[offset], sizeof(summary) - offset, "]");
        }
        server_runtime_store_outgoing_context(server_runtime, invoke_id, "GetNamedVariableListAttributes", summary);
    }
    server_runtime_set_diagnostic(diagnostic, UNITLAB_MMS_DIAGNOSTIC_OK, NULL);
    return 1;
}

