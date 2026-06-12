#ifndef UNITLAB_NATIVE_WIRE_CLIENT_SESSION_H
#define UNITLAB_NATIVE_WIRE_CLIENT_SESSION_H

#include <stddef.h>
#include <stdint.h>

enum {
    UNITLAB_NATIVE_DISCOVERY_MAX_RCBS = 256U,
    UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE = 32U,
    UNITLAB_NATIVE_DISCOVERY_MAX_INVOKE_SPAN = 2048U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_SET_CAPACITY = 16U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_SET_MEMBER_CAPACITY = 64U,
};

typedef struct {
    char domain[128U];
    size_t logical_device_count;
    size_t logical_node_count;
    size_t data_set_count;
    size_t data_set_member_count;
    size_t brcb_count;
    size_t last_report_data_ref_count;
    size_t last_report_value_count;
    size_t last_report_reason_count;
    size_t last_report_matched_data_ref_count;
    char last_report_rpt_id[160U];
    char last_report_data_set[160U];
} UnitLabNativeDiscoveredDeviceModel;

typedef struct {
    int rpt_enabled;
    int gi_requested;
    int last_report_received;
    size_t selected_rcb_index;
    uint32_t last_rptena_invoke_id;
    uint32_t last_gi_invoke_id;
    size_t async_report_count;
    char rcb_domain[128U];
    char rcb_item[320U];
} UnitLabNativeSubscriptionModel;

typedef struct {
    char reference[384U];
    size_t member_start;
    size_t member_count;
} UnitLabNativeDiscoveredDataSet;

typedef struct {
    UnitLabNativeDiscoveredDeviceModel discovered_model;
    UnitLabNativeSubscriptionModel subscription_model;
    UnitLabNativeDiscoveredDataSet* discovered_data_sets;
    size_t discovered_data_set_count;
    size_t discovered_data_set_capacity;
    char (*discovered_data_set_members)[384U];
    size_t discovered_data_set_member_count;
    size_t discovered_data_set_member_capacity;
} UnitLabNativeClientSessionState;

void unitlab_native_client_session_reset(UnitLabNativeClientSessionState* session);
int unitlab_native_client_session_data_set_member_exists(const UnitLabNativeClientSessionState* session, const char* reference);
int unitlab_native_client_session_data_set_index_by_reference(const UnitLabNativeClientSessionState* session, const char* reference, size_t* data_set_index);
int unitlab_native_client_session_data_set_contains_member(const UnitLabNativeClientSessionState* session, const char* data_set_reference, const char* member_reference);
UnitLabNativeDiscoveredDataSet* unitlab_native_client_session_append_data_set(UnitLabNativeClientSessionState* session, const char* data_set_reference);
int unitlab_native_client_session_append_data_set_member(UnitLabNativeClientSessionState* session, UnitLabNativeDiscoveredDataSet* data_set, const char* member_reference);

#endif
