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


int unitlab_native_client_session_append_data_component(UnitLabNativeClientSessionState* session, UnitLabNativeDiscoveredDataName* data_name, const char* component_name)
{
    if (session == NULL || data_name == NULL || component_name == NULL || component_name[0] == '\0') {
        return 0;
    }
    if (!ensure_data_component_capacity(session, session->discovered_data_component_count + 1U)) {
        return 0;
    }
    snprintf(session->discovered_data_components[session->discovered_data_component_count].name, sizeof(session->discovered_data_components[session->discovered_data_component_count].name), "%s", component_name);
    session->discovered_data_component_count++;
    data_name->component_count++;
    session->discovered_model.data_component_count = session->discovered_data_component_count;
    return 1;
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
