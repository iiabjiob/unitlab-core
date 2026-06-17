#include "native_wire_client_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ensure_logical_device_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredLogicalDevice* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_logical_device_capacity) {
        return 1;
    }
    new_capacity = session->discovered_logical_device_capacity != 0U ? session->discovered_logical_device_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_LOGICAL_DEVICE_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredLogicalDevice*)realloc(session->discovered_logical_devices, new_capacity * sizeof(session->discovered_logical_devices[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_logical_device_capacity) {
        memset(&resized[session->discovered_logical_device_capacity], 0, (new_capacity - session->discovered_logical_device_capacity) * sizeof(resized[0]));
    }
    session->discovered_logical_devices = resized;
    session->discovered_logical_device_capacity = new_capacity;
    return 1;
}

static int ensure_logical_node_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredLogicalNode* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_logical_node_capacity) {
        return 1;
    }
    new_capacity = session->discovered_logical_node_capacity != 0U ? session->discovered_logical_node_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_LOGICAL_NODE_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredLogicalNode*)realloc(session->discovered_logical_nodes, new_capacity * sizeof(session->discovered_logical_nodes[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_logical_node_capacity) {
        memset(&resized[session->discovered_logical_node_capacity], 0, (new_capacity - session->discovered_logical_node_capacity) * sizeof(resized[0]));
    }
    session->discovered_logical_nodes = resized;
    session->discovered_logical_node_capacity = new_capacity;
    return 1;
}

static int ensure_data_name_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredDataName* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_data_name_capacity) {
        return 1;
    }
    new_capacity = session->discovered_data_name_capacity != 0U ? session->discovered_data_name_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_NAME_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredDataName*)realloc(session->discovered_data_names, new_capacity * sizeof(session->discovered_data_names[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_data_name_capacity) {
        memset(&resized[session->discovered_data_name_capacity], 0, (new_capacity - session->discovered_data_name_capacity) * sizeof(resized[0]));
    }
    session->discovered_data_names = resized;
    session->discovered_data_name_capacity = new_capacity;
    return 1;
}


static int ensure_data_component_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredDataComponent* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_data_component_capacity) {
        return 1;
    }
    new_capacity = session->discovered_data_component_capacity != 0U ? session->discovered_data_component_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_COMPONENT_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredDataComponent*)realloc(session->discovered_data_components, new_capacity * sizeof(session->discovered_data_components[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_data_component_capacity) {
        memset(&resized[session->discovered_data_component_capacity], 0, (new_capacity - session->discovered_data_component_capacity) * sizeof(resized[0]));
    }
    session->discovered_data_components = resized;
    session->discovered_data_component_capacity = new_capacity;
    return 1;
}


static int ensure_typed_data_node_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredTypedDataNode* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_typed_data_node_capacity) {
        return 1;
    }
    new_capacity = session->discovered_typed_data_node_capacity != 0U ? session->discovered_typed_data_node_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_TYPED_NODE_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredTypedDataNode*)realloc(session->discovered_typed_data_nodes, new_capacity * sizeof(session->discovered_typed_data_nodes[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_typed_data_node_capacity) {
        memset(&resized[session->discovered_typed_data_node_capacity], 0, (new_capacity - session->discovered_typed_data_node_capacity) * sizeof(resized[0]));
    }
    session->discovered_typed_data_nodes = resized;
    session->discovered_typed_data_node_capacity = new_capacity;
    return 1;
}

static int ensure_leaf_ref_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredLeafRef* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_leaf_ref_capacity) {
        return 1;
    }
    new_capacity = session->discovered_leaf_ref_capacity != 0U ? session->discovered_leaf_ref_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_LEAF_REF_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredLeafRef*)realloc(session->discovered_leaf_refs, new_capacity * sizeof(session->discovered_leaf_refs[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_leaf_ref_capacity) {
        memset(&resized[session->discovered_leaf_ref_capacity], 0, (new_capacity - session->discovered_leaf_ref_capacity) * sizeof(resized[0]));
    }
    session->discovered_leaf_refs = resized;
    session->discovered_leaf_ref_capacity = new_capacity;
    return 1;
}

static int ensure_last_report_entry_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeLastReportEntry* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->last_report_entry_capacity) {
        return 1;
    }
    new_capacity = session->last_report_entry_capacity != 0U ? session->last_report_entry_capacity : UNITLAB_NATIVE_INITIAL_REPORT_ENTRY_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeLastReportEntry*)realloc(session->last_report_entries, new_capacity * sizeof(session->last_report_entries[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->last_report_entry_capacity) {
        memset(&resized[session->last_report_entry_capacity], 0, (new_capacity - session->last_report_entry_capacity) * sizeof(resized[0]));
    }
    session->last_report_entries = resized;
    session->last_report_entry_capacity = new_capacity;
    return 1;
}

static void normalize_path_dollars_to_dots(const char* input, char* output, size_t output_size)
{
    size_t index = 0U;

    if (output == NULL || output_size == 0U) {
        return;
    }
    output[0] = '\0';
    if (input == NULL) {
        return;
    }
    while (input[index] != '\0' && index + 1U < output_size) {
        output[index] = input[index] == '$' ? '.' : input[index];
        index++;
    }
    output[index] = '\0';
}


static void append_text(char* output, size_t output_size, const char* text)
{
    size_t used;
    size_t remaining;

    if (output == NULL || output_size == 0U || text == NULL) {
        return;
    }
    used = strlen(output);
    if (used >= output_size - 1U) {
        return;
    }
    remaining = output_size - used - 1U;
    strncat(output, text, remaining);
}

static int populate_leaf_ref_fields(UnitLabNativeDiscoveredLeafRef* leaf_ref, const char* mms_reference)
{
    char item[256U];
    char* slash;
    char* first_dollar;
    char* second_dollar;
    char dotted_path[192U];
    size_t domain_length;
    size_t ln_length;
    size_t fc_length;

    if (leaf_ref == NULL || mms_reference == NULL || mms_reference[0] == '\0') {
        return 0;
    }
    snprintf(leaf_ref->mms_reference, sizeof(leaf_ref->mms_reference), "%s", mms_reference);
    slash = strchr(leaf_ref->mms_reference, '/');
    if (slash == NULL || slash == leaf_ref->mms_reference || slash[1] == '\0') {
        snprintf(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), "%s", mms_reference);
        return 1;
    }
    domain_length = (size_t)(slash - leaf_ref->mms_reference);
    snprintf(leaf_ref->logical_device, sizeof(leaf_ref->logical_device), "%.*s", (int)domain_length, leaf_ref->mms_reference);
    snprintf(item, sizeof(item), "%s", slash + 1);
    first_dollar = strchr(item, '$');
    if (first_dollar == NULL || first_dollar == item || first_dollar[1] == '\0') {
        snprintf(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), "%s/%s", leaf_ref->logical_device, item);
        return 1;
    }
    ln_length = (size_t)(first_dollar - item);
    snprintf(leaf_ref->logical_node, sizeof(leaf_ref->logical_node), "%.*s", (int)ln_length, item);
    second_dollar = strchr(first_dollar + 1, '$');
    if (second_dollar == NULL || second_dollar == first_dollar + 1 || second_dollar[1] == '\0') {
        normalize_path_dollars_to_dots(item, dotted_path, sizeof(dotted_path));
        snprintf(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), "%s/%s", leaf_ref->logical_device, dotted_path);
        return 1;
    }
    fc_length = (size_t)(second_dollar - first_dollar - 1);
    snprintf(leaf_ref->fc, sizeof(leaf_ref->fc), "%.*s", (int)fc_length, first_dollar + 1);
    snprintf(leaf_ref->path, sizeof(leaf_ref->path), "%s", second_dollar + 1);
    normalize_path_dollars_to_dots(leaf_ref->path, dotted_path, sizeof(dotted_path));
    leaf_ref->display_reference[0] = '\0';
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), leaf_ref->logical_device);
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), "/");
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), leaf_ref->logical_node);
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), ".");
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), leaf_ref->fc);
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), ".");
    append_text(leaf_ref->display_reference, sizeof(leaf_ref->display_reference), dotted_path);
    return 1;
}

static int ensure_data_set_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredDataSet* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_data_set_capacity) {
        return 1;
    }
    new_capacity = session->discovered_data_set_capacity != 0U ? session->discovered_data_set_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_SET_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredDataSet*)realloc(session->discovered_data_sets, new_capacity * sizeof(session->discovered_data_sets[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_data_set_capacity) {
        memset(&resized[session->discovered_data_set_capacity], 0, (new_capacity - session->discovered_data_set_capacity) * sizeof(resized[0]));
    }
    session->discovered_data_sets = resized;
    session->discovered_data_set_capacity = new_capacity;
    return 1;
}

static int ensure_data_set_member_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    char (*resized)[384U];
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_data_set_member_capacity) {
        return 1;
    }
    new_capacity = session->discovered_data_set_member_capacity != 0U ? session->discovered_data_set_member_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_SET_MEMBER_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (char (*)[384U])realloc(session->discovered_data_set_members, new_capacity * sizeof(session->discovered_data_set_members[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_data_set_member_capacity) {
        memset(&resized[session->discovered_data_set_member_capacity], 0, (new_capacity - session->discovered_data_set_member_capacity) * sizeof(resized[0]));
    }
    session->discovered_data_set_members = resized;
    session->discovered_data_set_member_capacity = new_capacity;
    return 1;
}

static int ensure_discovered_rcb_capacity(UnitLabNativeClientSessionState* session, size_t required)
{
    UnitLabNativeDiscoveredRcb* resized;
    size_t new_capacity;

    if (session == NULL) {
        return 0;
    }
    if (required <= session->discovered_rcb_capacity) {
        return 1;
    }
    new_capacity = session->discovered_rcb_capacity != 0U ? session->discovered_rcb_capacity : UNITLAB_NATIVE_DISCOVERY_INITIAL_RCB_CAPACITY;
    while (new_capacity < required) {
        if (new_capacity > ((size_t)-1) / 2U) {
            return 0;
        }
        new_capacity *= 2U;
    }
    resized = (UnitLabNativeDiscoveredRcb*)realloc(session->discovered_rcbs, new_capacity * sizeof(session->discovered_rcbs[0]));
    if (resized == NULL) {
        return 0;
    }
    if (new_capacity > session->discovered_rcb_capacity) {
        memset(&resized[session->discovered_rcb_capacity], 0, (new_capacity - session->discovered_rcb_capacity) * sizeof(resized[0]));
    }
    session->discovered_rcbs = resized;
    session->discovered_rcb_capacity = new_capacity;
    return 1;
}

void unitlab_native_client_session_reset(UnitLabNativeClientSessionState* session)
{
    if (session == NULL) {
        return;
    }
    free(session->discovered_logical_devices);
    free(session->discovered_logical_nodes);
    free(session->discovered_data_names);
    free(session->discovered_data_components);
    free(session->discovered_typed_data_nodes);
    free(session->discovered_leaf_refs);
    free(session->last_report_entries);
    free(session->discovered_data_sets);
    free(session->discovered_data_set_members);
    free(session->discovered_rcbs);
    memset(session, 0, sizeof(*session));
}

UnitLabNativeDiscoveredLogicalDevice* unitlab_native_client_session_append_logical_device(UnitLabNativeClientSessionState* session, const char* name)
{
    UnitLabNativeDiscoveredLogicalDevice* logical_device;

    if (session == NULL || name == NULL || name[0] == '\0') {
        return NULL;
    }
    if (!ensure_logical_device_capacity(session, session->discovered_logical_device_count + 1U)) {
        return NULL;
    }
    logical_device = &session->discovered_logical_devices[session->discovered_logical_device_count];
    memset(logical_device, 0, sizeof(*logical_device));
    snprintf(logical_device->name, sizeof(logical_device->name), "%s", name);
    session->discovered_logical_device_count++;
    session->discovered_model.logical_device_count = session->discovered_logical_device_count;
    return logical_device;
}

UnitLabNativeDiscoveredLogicalNode* unitlab_native_client_session_append_logical_node(UnitLabNativeClientSessionState* session, const char* logical_device, const char* name)
{
    UnitLabNativeDiscoveredLogicalNode* logical_node;

    if (session == NULL || logical_device == NULL || logical_device[0] == '\0' || name == NULL || name[0] == '\0') {
        return NULL;
    }
    if (!ensure_logical_node_capacity(session, session->discovered_logical_node_count + 1U)) {
        return NULL;
    }
    logical_node = &session->discovered_logical_nodes[session->discovered_logical_node_count];
    memset(logical_node, 0, sizeof(*logical_node));
    snprintf(logical_node->logical_device, sizeof(logical_node->logical_device), "%s", logical_device);
    snprintf(logical_node->name, sizeof(logical_node->name), "%s", name);
    session->discovered_logical_node_count++;
    session->discovered_model.logical_node_count = session->discovered_logical_node_count;
    return logical_node;
}

UnitLabNativeDiscoveredDataName* unitlab_native_client_session_append_data_name(UnitLabNativeClientSessionState* session, const char* logical_device, const char* logical_node, const char* name)
{
    UnitLabNativeDiscoveredDataName* data_name;

    if (session == NULL || logical_device == NULL || logical_device[0] == '\0' || logical_node == NULL || logical_node[0] == '\0' || name == NULL || name[0] == '\0') {
        return NULL;
    }
    if (!ensure_data_name_capacity(session, session->discovered_data_name_count + 1U)) {
        return NULL;
    }
    data_name = &session->discovered_data_names[session->discovered_data_name_count];
    memset(data_name, 0, sizeof(*data_name));
    snprintf(data_name->logical_device, sizeof(data_name->logical_device), "%s", logical_device);
    snprintf(data_name->logical_node, sizeof(data_name->logical_node), "%s", logical_node);
    snprintf(data_name->name, sizeof(data_name->name), "%s", name);
    data_name->component_start = session->discovered_data_component_count;
    session->discovered_data_name_count++;
    session->discovered_model.data_name_count = session->discovered_data_name_count;
    return data_name;
}


void unitlab_native_client_session_set_data_name_type(UnitLabNativeDiscoveredDataName* data_name, const char* type_kind)
{
    if (data_name == NULL) {
        return;
    }
    snprintf(data_name->type_kind, sizeof(data_name->type_kind), "%s", type_kind != NULL && type_kind[0] != '\0' ? type_kind : "unknown");
}

int unitlab_native_client_session_append_data_component(UnitLabNativeClientSessionState* session, UnitLabNativeDiscoveredDataName* data_name, const char* component_name, const char* type_kind)
{
    if (session == NULL || data_name == NULL || component_name == NULL || component_name[0] == '\0') {
        return 0;
    }
    if (!ensure_data_component_capacity(session, session->discovered_data_component_count + 1U)) {
        return 0;
    }
    snprintf(session->discovered_data_components[session->discovered_data_component_count].name, sizeof(session->discovered_data_components[session->discovered_data_component_count].name), "%s", component_name);
    snprintf(session->discovered_data_components[session->discovered_data_component_count].type_kind, sizeof(session->discovered_data_components[session->discovered_data_component_count].type_kind), "%s", type_kind != NULL && type_kind[0] != '\0' ? type_kind : "unknown");
    session->discovered_data_component_count++;
    data_name->component_count++;
    session->discovered_model.data_component_count = session->discovered_data_component_count;
    return 1;
}


UnitLabNativeDiscoveredTypedDataNode* unitlab_native_client_session_append_typed_data_node(UnitLabNativeClientSessionState* session, const char* logical_device, const char* logical_node, const char* fc, const char* path, const char* mms_reference, const char* display_reference, const char* type_kind, const char* node_kind, size_t depth, size_t parent_index)
{
    UnitLabNativeDiscoveredTypedDataNode* node;

    if (session == NULL || logical_device == NULL || logical_device[0] == '\0' || logical_node == NULL || logical_node[0] == '\0' || path == NULL || path[0] == '\0') {
        return NULL;
    }
    if (mms_reference != NULL && mms_reference[0] != '\0') {
        for (size_t index = 0U; index < session->discovered_typed_data_node_count; index++) {
            if (strcmp(session->discovered_typed_data_nodes[index].mms_reference, mms_reference) == 0) {
                return &session->discovered_typed_data_nodes[index];
            }
        }
    }
    if (!ensure_typed_data_node_capacity(session, session->discovered_typed_data_node_count + 1U)) {
        return NULL;
    }
    node = &session->discovered_typed_data_nodes[session->discovered_typed_data_node_count];
    memset(node, 0, sizeof(*node));
    snprintf(node->logical_device, sizeof(node->logical_device), "%s", logical_device);
    snprintf(node->logical_node, sizeof(node->logical_node), "%s", logical_node);
    snprintf(node->fc, sizeof(node->fc), "%s", fc != NULL ? fc : "");
    snprintf(node->path, sizeof(node->path), "%s", path);
    if (mms_reference != NULL && mms_reference[0] != '\0') {
        snprintf(node->mms_reference, sizeof(node->mms_reference), "%s", mms_reference);
    }
    if (display_reference != NULL && display_reference[0] != '\0') {
        snprintf(node->display_reference, sizeof(node->display_reference), "%s", display_reference);
    }
    snprintf(node->request_item, sizeof(node->request_item), "%s", path);
    snprintf(node->reference_kind, sizeof(node->reference_kind), "%s", fc != NULL && fc[0] != '\0' ? "fc-context" : "ln-context");
    snprintf(node->semantic_kind, sizeof(node->semantic_kind), "%s", node_kind != NULL && node_kind[0] != '\0' ? node_kind : "branch");
    snprintf(node->type_kind, sizeof(node->type_kind), "%s", type_kind != NULL && type_kind[0] != '\0' ? type_kind : "unknown");
    snprintf(node->node_kind, sizeof(node->node_kind), "%s", node_kind != NULL && node_kind[0] != '\0' ? node_kind : "branch");
    node->depth = depth;
    node->parent_index = parent_index;
    node->child_count = 0U;
    session->discovered_typed_data_node_count++;
    session->discovered_model.typed_data_node_count = session->discovered_typed_data_node_count;
    if (parent_index != (size_t)-1 && parent_index < session->discovered_typed_data_node_count - 1U) {
        session->discovered_typed_data_nodes[parent_index].child_count++;
    }
    return node;
}

const UnitLabNativeDiscoveredTypedDataNode* unitlab_native_client_session_typed_data_node_at(const UnitLabNativeClientSessionState* session, size_t index)
{
    if (session == NULL || index >= session->discovered_typed_data_node_count) {
        return NULL;
    }
    return &session->discovered_typed_data_nodes[index];
}

UnitLabNativeDiscoveredLeafRef* unitlab_native_client_session_append_leaf_ref(UnitLabNativeClientSessionState* session, const char* mms_reference)
{
    UnitLabNativeDiscoveredLeafRef* leaf_ref;

    if (session == NULL || mms_reference == NULL || mms_reference[0] == '\0') {
        return NULL;
    }
    if (unitlab_native_client_session_leaf_ref_exists(session, mms_reference)) {
        for (size_t index = 0U; index < session->discovered_leaf_ref_count; index++) {
            if (strcmp(session->discovered_leaf_refs[index].mms_reference, mms_reference) == 0) {
                return &session->discovered_leaf_refs[index];
            }
        }
    }
    if (!ensure_leaf_ref_capacity(session, session->discovered_leaf_ref_count + 1U)) {
        return NULL;
    }
    leaf_ref = &session->discovered_leaf_refs[session->discovered_leaf_ref_count];
    memset(leaf_ref, 0, sizeof(*leaf_ref));
    if (!populate_leaf_ref_fields(leaf_ref, mms_reference)) {
        return NULL;
    }
    session->discovered_leaf_ref_count++;
    session->discovered_model.leaf_ref_count = session->discovered_leaf_ref_count;
    return leaf_ref;
}

int unitlab_native_client_session_leaf_ref_exists(const UnitLabNativeClientSessionState* session, const char* mms_reference)
{
    if (session == NULL || mms_reference == NULL || mms_reference[0] == '\0') {
        return 0;
    }
    for (size_t index = 0U; index < session->discovered_leaf_ref_count; index++) {
        if (strcmp(session->discovered_leaf_refs[index].mms_reference, mms_reference) == 0) {
            return 1;
        }
    }
    return 0;
}

const UnitLabNativeDiscoveredLeafRef* unitlab_native_client_session_find_leaf_ref(const UnitLabNativeClientSessionState* session, const char* mms_reference)
{
    if (session == NULL || mms_reference == NULL || mms_reference[0] == '\0') {
        return NULL;
    }
    for (size_t index = 0U; index < session->discovered_leaf_ref_count; index++) {
        if (strcmp(session->discovered_leaf_refs[index].mms_reference, mms_reference) == 0) {
            return &session->discovered_leaf_refs[index];
        }
    }
    return NULL;
}

void unitlab_native_client_session_reset_last_report(UnitLabNativeClientSessionState* session)
{
    if (session == NULL) {
        return;
    }
    if (session->last_report_entries != NULL && session->last_report_entry_capacity > 0U) {
        memset(session->last_report_entries, 0, session->last_report_entry_capacity * sizeof(session->last_report_entries[0]));
    }
    session->last_report_entry_count = 0U;
    session->discovered_model.last_report_data_ref_count = 0U;
    session->discovered_model.last_report_value_count = 0U;
    session->discovered_model.last_report_reason_count = 0U;
    session->discovered_model.last_report_matched_data_ref_count = 0U;
    session->discovered_model.last_report_dataset_mismatch_count = 0U;
    session->discovered_model.last_report_missing_value_count = 0U;
    session->discovered_model.last_report_extra_value_count = 0U;
    session->discovered_model.last_report_missing_reason_count = 0U;
    session->discovered_model.last_report_extra_reason_count = 0U;
    session->discovered_model.last_report_unsupported_value_count = 0U;
    session->discovered_model.last_report_rpt_id[0] = '\0';
    session->discovered_model.last_report_data_set[0] = '\0';
}

UnitLabNativeLastReportEntry* unitlab_native_client_session_append_last_report_entry(UnitLabNativeClientSessionState* session, const char* data_reference, int dataset_match, size_t inclusion_index)
{
    UnitLabNativeLastReportEntry* entry;
    const UnitLabNativeDiscoveredLeafRef* leaf_ref;

    if (session == NULL || data_reference == NULL || data_reference[0] == '\0') {
        return NULL;
    }
    if (!ensure_last_report_entry_capacity(session, session->last_report_entry_count + 1U)) {
        return NULL;
    }
    entry = &session->last_report_entries[session->last_report_entry_count];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->data_reference, sizeof(entry->data_reference), "%s", data_reference);
    entry->inclusion_index = inclusion_index;
    entry->dataset_match = dataset_match ? 1 : 0;
    leaf_ref = unitlab_native_client_session_find_leaf_ref(session, data_reference);
    if (leaf_ref != NULL) {
        snprintf(entry->display_reference, sizeof(entry->display_reference), "%s", leaf_ref->display_reference);
        snprintf(entry->type_kind, sizeof(entry->type_kind), "%s", leaf_ref->type_kind[0] != '\0' ? leaf_ref->type_kind : "unknown");
        entry->discovered_match = 1;
    } else {
        snprintf(entry->display_reference, sizeof(entry->display_reference), "%s", data_reference);
        snprintf(entry->type_kind, sizeof(entry->type_kind), "%s", "unknown");
    }
    session->last_report_entry_count++;
    return entry;
}

int unitlab_native_client_session_data_set_member_exists(const UnitLabNativeClientSessionState* session, const char* reference)
{
    if (session == NULL || reference == NULL || reference[0] == '\0') {
        return 0;
    }
    for (size_t index = 0U; index < session->discovered_data_set_member_count; index++) {
        if (strcmp(session->discovered_data_set_members[index], reference) == 0) {
            return 1;
        }
    }
    return 0;
}

int unitlab_native_client_session_data_set_index_by_reference(const UnitLabNativeClientSessionState* session, const char* reference, size_t* data_set_index)
{
    if (session == NULL || reference == NULL || reference[0] == '\0') {
        return 0;
    }
    for (size_t index = 0U; index < session->discovered_data_set_count; index++) {
        if (strcmp(session->discovered_data_sets[index].reference, reference) == 0) {
            if (data_set_index != NULL) {
                *data_set_index = index;
            }
            return 1;
        }
    }
    return 0;
}

int unitlab_native_client_session_data_set_contains_member(const UnitLabNativeClientSessionState* session, const char* data_set_reference, const char* member_reference)
{
    size_t data_set_index = 0U;
    const UnitLabNativeDiscoveredDataSet* data_set;

    if (!unitlab_native_client_session_data_set_index_by_reference(session, data_set_reference, &data_set_index) || member_reference == NULL || member_reference[0] == '\0') {
        return 0;
    }
    data_set = &session->discovered_data_sets[data_set_index];
    for (size_t index = 0U; index < data_set->member_count; index++) {
        size_t member_index = data_set->member_start + index;
        if (member_index < session->discovered_data_set_member_count && strcmp(session->discovered_data_set_members[member_index], member_reference) == 0) {
            return 1;
        }
    }
    return 0;
}

const char* unitlab_native_client_session_data_set_member_at(const UnitLabNativeClientSessionState* session, const char* data_set_reference, size_t member_index)
{
    size_t data_set_index = 0U;
    const UnitLabNativeDiscoveredDataSet* data_set;
    size_t absolute_member_index;

    if (!unitlab_native_client_session_data_set_index_by_reference(session, data_set_reference, &data_set_index)) {
        return NULL;
    }
    data_set = &session->discovered_data_sets[data_set_index];
    if (member_index >= data_set->member_count) {
        return NULL;
    }
    absolute_member_index = data_set->member_start + member_index;
    if (absolute_member_index >= session->discovered_data_set_member_count) {
        return NULL;
    }
    return session->discovered_data_set_members[absolute_member_index];
}

UnitLabNativeDiscoveredDataSet* unitlab_native_client_session_append_data_set(UnitLabNativeClientSessionState* session, const char* data_set_reference)
{
    UnitLabNativeDiscoveredDataSet* data_set;

    if (session == NULL || data_set_reference == NULL || data_set_reference[0] == '\0') {
        return NULL;
    }
    if (!ensure_data_set_capacity(session, session->discovered_data_set_count + 1U)) {
        return NULL;
    }
    data_set = &session->discovered_data_sets[session->discovered_data_set_count];
    memset(data_set, 0, sizeof(*data_set));
    snprintf(data_set->reference, sizeof(data_set->reference), "%s", data_set_reference);
    data_set->member_start = session->discovered_data_set_member_count;
    session->discovered_data_set_count++;
    session->discovered_model.data_set_count = session->discovered_data_set_count;
    return data_set;
}

int unitlab_native_client_session_append_data_set_member(UnitLabNativeClientSessionState* session, UnitLabNativeDiscoveredDataSet* data_set, const char* member_reference)
{
    if (session == NULL || data_set == NULL || member_reference == NULL || member_reference[0] == '\0') {
        return 0;
    }
    if (!ensure_data_set_member_capacity(session, session->discovered_data_set_member_count + 1U)) {
        return 0;
    }
    snprintf(session->discovered_data_set_members[session->discovered_data_set_member_count], sizeof(session->discovered_data_set_members[session->discovered_data_set_member_count]), "%s", member_reference);
    session->discovered_data_set_member_count++;
    data_set->member_count++;
    session->discovered_model.data_set_member_count = session->discovered_data_set_member_count;
    return 1;
}

UnitLabNativeDiscoveredRcb* unitlab_native_client_session_append_discovered_rcb(UnitLabNativeClientSessionState* session, const char* domain, const char* item)
{
    UnitLabNativeDiscoveredRcb* rcb;

    if (session == NULL || domain == NULL || domain[0] == '\0' || item == NULL || item[0] == '\0') {
        return NULL;
    }
    if (!ensure_discovered_rcb_capacity(session, session->discovered_rcb_count + 1U)) {
        return NULL;
    }
    rcb = &session->discovered_rcbs[session->discovered_rcb_count];
    memset(rcb, 0, sizeof(*rcb));
    snprintf(rcb->domain, sizeof(rcb->domain), "%s", domain);
    snprintf(rcb->item, sizeof(rcb->item), "%s", item);
    session->discovered_rcb_count++;
    session->discovered_model.brcb_count = session->discovered_rcb_count;
    return rcb;
}

const UnitLabNativeDiscoveredRcb* unitlab_native_client_session_discovered_rcb_at(const UnitLabNativeClientSessionState* session, size_t index)
{
    if (session == NULL || index >= session->discovered_rcb_count) {
        return NULL;
    }
    return &session->discovered_rcbs[index];
}
