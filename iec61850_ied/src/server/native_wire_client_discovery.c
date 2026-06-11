#include "native_wire_client_discovery.h"

#include <stdio.h>
#include <string.h>

#include "native_wire_client_ber_helpers.h"
#include "wire/ber/unitlab_mms_ber.h"
#include "wire/mms/unitlab_mms_pdu.h"
#include "wire/orchestration/unitlab_mms_association_frame.h"

static size_t collect_get_named_variable_list_members_from_frame(UnitLabNativeClientSessionState* session, const char* data_set_reference, const uint8_t* frame, size_t frame_length)
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
        return 0U;
    }
    while (offset < list.value_length && session->discovered_data_set_member_count < sizeof(session->discovered_data_set_members) / sizeof(session->discovered_data_set_members[0])) {
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
            snprintf(session->discovered_data_set_members[session->discovered_data_set_member_count], sizeof(session->discovered_data_set_members[session->discovered_data_set_member_count]), "%s", reference);
            printf(
                "native-wire-client: discovered-dataset-member[%zu.%zu] dataset=%s ref=%s\n",
                session->discovered_data_set_count - 1U,
                data_set->member_count,
                data_set->reference,
                session->discovered_data_set_members[session->discovered_data_set_member_count]);
            fflush(stdout);
            session->discovered_data_set_member_count++;
            data_set->member_count++;
            session->discovered_model.data_set_member_count = session->discovered_data_set_member_count;
            added++;
        }
        offset += member_consumed;
    }
    return added;
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
            && unitlab_native_client_bytes_are_printable_ascii(item_element.value_bytes, item_element.value_length)) {
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

int unitlab_native_client_run_discover_sequence(
    UnitLabNativeClientSessionState* session,
    const UnitLabNativeDiscoveryIo* io,
    const char* domain_id,
    uint32_t invoke_id,
    uint32_t* next_invoke_id,
    char* discovered_domain,
    size_t discovered_domain_size,
    char discovered_brcb_items[][320U],
    size_t max_discovered_brcb_items,
    size_t* discovered_brcb_count)
{
    if (session == NULL || io == NULL || io->get_name_list_step == NULL || io->read_step == NULL || io->attributes_step == NULL || domain_id == NULL || domain_id[0] == '\0' || next_invoke_id == NULL || discovered_domain == NULL || discovered_domain_size == 0U || discovered_brcb_items == NULL || discovered_brcb_count == NULL) {
        if (io != NULL && io->diagnostic != NULL) {
            io->diagnostic->code = UNITLAB_MMS_DIAGNOSTIC_INVALID_ARGUMENT;
            snprintf(io->diagnostic->message, sizeof(io->diagnostic->message), "%s", "Native wire client discover sequence requires session, domain, and discovery buffers.");
        }
        return 0;
    }
    char logical_device_names[UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_DEVICES][128U];
    char logical_node_names[UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_NODES][128U];
    char data_set_items[UNITLAB_NATIVE_DISCOVERY_MAX_DATA_SETS][128U];
    char brcb_names[UNITLAB_NATIVE_DISCOVERY_MAX_RCBS][128U];
    char brcb_logical_nodes[UNITLAB_NATIVE_DISCOVERY_MAX_RCBS][128U];
    size_t logical_node_count = 0U;
    size_t data_set_count = 0U;
    size_t brcb_count = 0U;
    uint32_t followup_invoke_id = invoke_id + 2U;
    int more_follows = 0;
    char last_identifier[128U];

    discovered_domain[0] = '\0';
    memset(discovered_brcb_items, 0, max_discovered_brcb_items * 320U);
    *discovered_brcb_count = 0U;
    unitlab_native_client_session_reset(session);
    snprintf(session->discovered_model.domain, sizeof(session->discovered_model.domain), "%s", domain_id);

    if (!io->get_name_list_step(session, io, "vmd-logical-devices", 9U, 0U, NULL, NULL, invoke_id)) {
        return 0;
    }
    session->discovered_model.logical_device_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, logical_device_names, UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_DEVICES, &more_follows, last_identifier, sizeof(last_identifier));
    if (!io->get_name_list_step(session, io, "domain-logical-nodes", 1U, 1U, domain_id, NULL, invoke_id + 1U)) {
        return 0;
    }
    logical_node_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, logical_node_names, UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_NODES, &more_follows, last_identifier, sizeof(last_identifier));
    while (more_follows && logical_node_count < UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_NODES && last_identifier[0] != '\0') {
        char page_items[UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE][128U];
        size_t page_count;
        if (!io->get_name_list_step(session, io, "domain-logical-nodes-page", 1U, 1U, domain_id, last_identifier, followup_invoke_id++)) {
            return 0;
        }
        page_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, page_items, UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE, &more_follows, last_identifier, sizeof(last_identifier));
        for (size_t page_index = 0U; page_index < page_count && logical_node_count < UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_NODES; page_index++) {
            snprintf(logical_node_names[logical_node_count], sizeof(logical_node_names[logical_node_count]), "%s", page_items[page_index]);
            logical_node_count++;
        }
    }
    if (more_follows) {
        printf("native-wire-client: discover-truncated=logical-nodes limit=%u continue-after=%s\n", (unsigned)UNITLAB_NATIVE_DISCOVERY_MAX_LOGICAL_NODES, last_identifier[0] != '\0' ? last_identifier : "<none>");
        fflush(stdout);
    }
    if (!io->get_name_list_step(session, io, "domain-datasets", 2U, 1U, domain_id, NULL, followup_invoke_id++)) {
        return 0;
    }
    data_set_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, data_set_items, UNITLAB_NATIVE_DISCOVERY_MAX_DATA_SETS, &more_follows, last_identifier, sizeof(last_identifier));
    while (more_follows && data_set_count < UNITLAB_NATIVE_DISCOVERY_MAX_DATA_SETS && last_identifier[0] != '\0') {
        char page_items[UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE][128U];
        size_t page_count;
        if (!io->get_name_list_step(session, io, "domain-datasets-page", 2U, 1U, domain_id, last_identifier, followup_invoke_id++)) {
            return 0;
        }
        page_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, page_items, UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE, &more_follows, last_identifier, sizeof(last_identifier));
        for (size_t page_index = 0U; page_index < page_count && data_set_count < UNITLAB_NATIVE_DISCOVERY_MAX_DATA_SETS; page_index++) {
            snprintf(data_set_items[data_set_count], sizeof(data_set_items[data_set_count]), "%s", page_items[page_index]);
            data_set_count++;
        }
    }
    if (more_follows) {
        printf("native-wire-client: discover-truncated=datasets limit=%u continue-after=%s\n", (unsigned)UNITLAB_NATIVE_DISCOVERY_MAX_DATA_SETS, last_identifier[0] != '\0' ? last_identifier : "<none>");
        fflush(stdout);
    }
    session->discovered_model.data_set_count = data_set_count;
    if (logical_node_count == 0U) {
        snprintf(logical_node_names[0], sizeof(logical_node_names[0]), "%s", "LLN0");
        logical_node_count = 1U;
        printf("native-wire-client: discover-fallback=logical-nodes value=LLN0\n");
        fflush(stdout);
    }
    session->discovered_model.logical_node_count = logical_node_count;
    for (size_t ln_index = 0U; ln_index < logical_node_count; ln_index++) {
        char step_label[160U];
        char ln_brcb_names[UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE][128U];
        size_t ln_brcb_count = 0U;

        snprintf(step_label, sizeof(step_label), "ln-data-attributes:%s", logical_node_names[ln_index]);
        if (!io->get_name_list_step(session, io, step_label, 3U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++)) {
            return 0;
        }
        snprintf(step_label, sizeof(step_label), "ln-brcbs:%s", logical_node_names[ln_index]);
        if (!io->get_name_list_step(session, io, step_label, 4U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++)) {
            return 0;
        }
        ln_brcb_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, ln_brcb_names, UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE, &more_follows, last_identifier, sizeof(last_identifier));
        for (size_t brcb_index = 0U; brcb_index < ln_brcb_count && brcb_count < UNITLAB_NATIVE_DISCOVERY_MAX_RCBS; brcb_index++) {
            snprintf(brcb_names[brcb_count], sizeof(brcb_names[brcb_count]), "%s", ln_brcb_names[brcb_index]);
            snprintf(brcb_logical_nodes[brcb_count], sizeof(brcb_logical_nodes[brcb_count]), "%s", logical_node_names[ln_index]);
            brcb_count++;
        }
        while (more_follows && brcb_count < UNITLAB_NATIVE_DISCOVERY_MAX_RCBS && last_identifier[0] != '\0') {
            size_t page_count;
            snprintf(step_label, sizeof(step_label), "ln-brcbs-page:%s", logical_node_names[ln_index]);
            if (!io->get_name_list_step(session, io, step_label, 4U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++)) {
                return 0;
            }
            page_count = extract_get_name_list_identifiers(io->response, *io->encoded_response_length, ln_brcb_names, UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE, &more_follows, last_identifier, sizeof(last_identifier));
            for (size_t brcb_index = 0U; brcb_index < page_count && brcb_count < UNITLAB_NATIVE_DISCOVERY_MAX_RCBS; brcb_index++) {
                snprintf(brcb_names[brcb_count], sizeof(brcb_names[brcb_count]), "%s", ln_brcb_names[brcb_index]);
                snprintf(brcb_logical_nodes[brcb_count], sizeof(brcb_logical_nodes[brcb_count]), "%s", logical_node_names[ln_index]);
                brcb_count++;
            }
        }
        if (more_follows) {
            printf("native-wire-client: discover-truncated=ln-brcbs:%s limit=%u continue-after=%s\n", logical_node_names[ln_index], (unsigned)UNITLAB_NATIVE_DISCOVERY_MAX_RCBS, last_identifier[0] != '\0' ? last_identifier : "<none>");
            fflush(stdout);
        }
        snprintf(step_label, sizeof(step_label), "ln-urcbs:%s", logical_node_names[ln_index]);
        if (!io->get_name_list_step(session, io, step_label, 5U, 1U, domain_id, logical_node_names[ln_index], followup_invoke_id++)) {
            return 0;
        }
    }
    session->discovered_model.brcb_count = brcb_count;
    if (brcb_count == 0U) {
        printf("native-wire-client: discover-skip=brcb-attrs reason=no-brcb\n");
        fflush(stdout);
    }
    for (size_t index = 0U; index < brcb_count; index++) {
        char brcb_item[320U];
        char brcb_read_item[320U];
        snprintf(brcb_item, sizeof(brcb_item), "%s$BR$%s$RptEna", brcb_logical_nodes[index], brcb_names[index]);
        if (!io->attributes_step(session, io, "brcb-attrs", domain_id, brcb_item, followup_invoke_id++, 0)) {
            return 0;
        }
        snprintf(brcb_read_item, sizeof(brcb_read_item), "%s$BR$%s", brcb_logical_nodes[index], brcb_names[index]);
        if (*discovered_brcb_count < max_discovered_brcb_items) {
            snprintf(discovered_domain, discovered_domain_size, "%s", domain_id);
            snprintf(discovered_brcb_items[*discovered_brcb_count], sizeof(discovered_brcb_items[*discovered_brcb_count]), "%s", brcb_read_item);
            printf("native-wire-client: discovered-brcb[%zu] domain=%s item=%s\n", *discovered_brcb_count, discovered_domain, discovered_brcb_items[*discovered_brcb_count]);
            fflush(stdout);
            (*discovered_brcb_count)++;
        }
        if (!io->read_step(session, io, "brcb-values", domain_id, brcb_read_item, followup_invoke_id++)) {
            return 0;
        }
    }
    if (data_set_count == 0U) {
        printf("native-wire-client: discover-skip=dataset-members reason=no-dataset\n");
        fflush(stdout);
    }
    for (size_t index = 0U; index < data_set_count; index++) {
        char data_set_reference[384U];
        snprintf(data_set_reference, sizeof(data_set_reference), "%s/%s", domain_id, data_set_items[index]);
        if (!io->attributes_step(session, io, "dataset-members", domain_id, data_set_items[index], followup_invoke_id++, 1)) {
            return 0;
        }
        (void)collect_get_named_variable_list_members_from_frame(session, data_set_reference, io->response, *io->encoded_response_length);
    }
    if (next_invoke_id != NULL) {
        *next_invoke_id = followup_invoke_id;
    }
    if (io->emit_model_summary != NULL) {
        io->emit_model_summary(session, "discover");
    }
    return 1;
}
