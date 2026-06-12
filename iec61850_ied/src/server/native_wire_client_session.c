#include "native_wire_client_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void unitlab_native_client_session_reset(UnitLabNativeClientSessionState* session)
{
    if (session == NULL) {
        return;
    }
    free(session->discovered_data_sets);
    free(session->discovered_data_set_members);
    memset(session, 0, sizeof(*session));
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
