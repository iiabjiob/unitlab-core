#include "native_wire_client_session.h"

#include <stdio.h>
#include <string.h>

void unitlab_native_client_session_reset(UnitLabNativeClientSessionState* session)
{
    if (session == NULL) {
        return;
    }
    memset(&session->discovered_model, 0, sizeof(session->discovered_model));
    memset(&session->subscription_model, 0, sizeof(session->subscription_model));
    memset(session->discovered_data_sets, 0, sizeof(session->discovered_data_sets));
    session->discovered_data_set_count = 0U;
    memset(session->discovered_data_set_members, 0, sizeof(session->discovered_data_set_members));
    session->discovered_data_set_member_count = 0U;
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

    if (session == NULL || data_set_reference == NULL || data_set_reference[0] == '\0' || session->discovered_data_set_count >= UNITLAB_NATIVE_DISCOVERY_MAX_DATA_SETS) {
        return NULL;
    }
    data_set = &session->discovered_data_sets[session->discovered_data_set_count];
    memset(data_set, 0, sizeof(*data_set));
    snprintf(data_set->reference, sizeof(data_set->reference), "%s", data_set_reference);
    data_set->member_start = session->discovered_data_set_member_count;
    session->discovered_data_set_count++;
    return data_set;
}
