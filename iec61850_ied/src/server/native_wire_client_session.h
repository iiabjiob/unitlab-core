#ifndef UNITLAB_NATIVE_WIRE_CLIENT_SESSION_H
#define UNITLAB_NATIVE_WIRE_CLIENT_SESSION_H

#include <stddef.h>
#include <stdint.h>

enum {
    UNITLAB_NATIVE_DISCOVERY_PAGE_SIZE = 32U,
    UNITLAB_NATIVE_DISCOVERY_MAX_INVOKE_SPAN = 2048U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_LOGICAL_DEVICE_CAPACITY = 8U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_LOGICAL_NODE_CAPACITY = 32U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_NAME_CAPACITY = 64U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_COMPONENT_CAPACITY = 128U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_LEAF_REF_CAPACITY = 128U,
    UNITLAB_NATIVE_INITIAL_REPORT_ENTRY_CAPACITY = 32U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_SET_CAPACITY = 16U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_DATA_SET_MEMBER_CAPACITY = 64U,
    UNITLAB_NATIVE_DISCOVERY_INITIAL_RCB_CAPACITY = 16U,
};

typedef struct {
    char domain[128U];
    size_t logical_device_count;
    size_t logical_node_count;
    size_t data_name_count;
    size_t data_component_count;
    size_t leaf_ref_count;
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
    char name[128U];
} UnitLabNativeDiscoveredLogicalDevice;

typedef struct {
    char logical_device[128U];
    char name[128U];
} UnitLabNativeDiscoveredLogicalNode;

typedef struct {
    char logical_device[128U];
    char logical_node[128U];
    char name[128U];
    char type_kind[32U];
    size_t component_start;
    size_t component_count;
} UnitLabNativeDiscoveredDataName;

typedef struct {
    char name[128U];
    char type_kind[32U];
} UnitLabNativeDiscoveredDataComponent;

typedef struct {
    char mms_reference[384U];
    char display_reference[384U];
    char logical_device[128U];
    char logical_node[128U];
    char fc[32U];
    char path[192U];
    char type_kind[32U];
    int matched_discovery;
} UnitLabNativeDiscoveredLeafRef;

typedef enum {
    UNITLAB_NATIVE_REPORT_VALUE_EMPTY = 0,
    UNITLAB_NATIVE_REPORT_VALUE_BOOL,
    UNITLAB_NATIVE_REPORT_VALUE_UNSIGNED,
    UNITLAB_NATIVE_REPORT_VALUE_INTEGER,
    UNITLAB_NATIVE_REPORT_VALUE_FLOAT,
    UNITLAB_NATIVE_REPORT_VALUE_STRING,
    UNITLAB_NATIVE_REPORT_VALUE_OCTETS,
    UNITLAB_NATIVE_REPORT_VALUE_BIT_STRING,
    UNITLAB_NATIVE_REPORT_VALUE_STRUCTURE,
    UNITLAB_NATIVE_REPORT_VALUE_UNSUPPORTED
} UnitLabNativeReportValueKind;

typedef struct {
    char data_reference[384U];
    char display_reference[384U];
    char value_summary[160U];
    char reason_summary[64U];
    char type_kind[32U];
    uint8_t raw_tag_class;
    uint8_t raw_tag_number;
    size_t raw_value_length;
    uint8_t raw_reason_tag_class;
    uint8_t raw_reason_tag_number;
    size_t raw_reason_length;
    uint32_t reason_code;
    UnitLabNativeReportValueKind value_kind;
    uint64_t unsigned_value;
    int64_t integer_value;
    double floating_value;
    int bool_value;
    size_t inclusion_index;
    int discovered_match;
    int dataset_match;
} UnitLabNativeLastReportEntry;

typedef struct {
    char reference[384U];
    size_t member_start;
    size_t member_count;
} UnitLabNativeDiscoveredDataSet;

typedef struct {
    char domain[128U];
    char item[320U];
} UnitLabNativeDiscoveredRcb;

typedef struct {
    UnitLabNativeDiscoveredDeviceModel discovered_model;
    UnitLabNativeSubscriptionModel subscription_model;
    UnitLabNativeDiscoveredLogicalDevice* discovered_logical_devices;
    size_t discovered_logical_device_count;
    size_t discovered_logical_device_capacity;
    UnitLabNativeDiscoveredLogicalNode* discovered_logical_nodes;
    size_t discovered_logical_node_count;
    size_t discovered_logical_node_capacity;
    UnitLabNativeDiscoveredDataName* discovered_data_names;
    size_t discovered_data_name_count;
    size_t discovered_data_name_capacity;
    UnitLabNativeDiscoveredDataComponent* discovered_data_components;
    size_t discovered_data_component_count;
    size_t discovered_data_component_capacity;
    UnitLabNativeDiscoveredLeafRef* discovered_leaf_refs;
    size_t discovered_leaf_ref_count;
    size_t discovered_leaf_ref_capacity;
    UnitLabNativeLastReportEntry* last_report_entries;
    size_t last_report_entry_count;
    size_t last_report_entry_capacity;
    UnitLabNativeDiscoveredDataSet* discovered_data_sets;
    size_t discovered_data_set_count;
    size_t discovered_data_set_capacity;
    char (*discovered_data_set_members)[384U];
    size_t discovered_data_set_member_count;
    size_t discovered_data_set_member_capacity;
    UnitLabNativeDiscoveredRcb* discovered_rcbs;
    size_t discovered_rcb_count;
    size_t discovered_rcb_capacity;
} UnitLabNativeClientSessionState;

void unitlab_native_client_session_reset(UnitLabNativeClientSessionState* session);
UnitLabNativeDiscoveredLogicalDevice* unitlab_native_client_session_append_logical_device(UnitLabNativeClientSessionState* session, const char* name);
UnitLabNativeDiscoveredLogicalNode* unitlab_native_client_session_append_logical_node(UnitLabNativeClientSessionState* session, const char* logical_device, const char* name);
UnitLabNativeDiscoveredDataName* unitlab_native_client_session_append_data_name(UnitLabNativeClientSessionState* session, const char* logical_device, const char* logical_node, const char* name);
void unitlab_native_client_session_set_data_name_type(UnitLabNativeDiscoveredDataName* data_name, const char* type_kind);
int unitlab_native_client_session_append_data_component(UnitLabNativeClientSessionState* session, UnitLabNativeDiscoveredDataName* data_name, const char* component_name, const char* type_kind);
UnitLabNativeDiscoveredLeafRef* unitlab_native_client_session_append_leaf_ref(UnitLabNativeClientSessionState* session, const char* mms_reference);
int unitlab_native_client_session_leaf_ref_exists(const UnitLabNativeClientSessionState* session, const char* mms_reference);
const UnitLabNativeDiscoveredLeafRef* unitlab_native_client_session_find_leaf_ref(const UnitLabNativeClientSessionState* session, const char* mms_reference);
void unitlab_native_client_session_reset_last_report(UnitLabNativeClientSessionState* session);
UnitLabNativeLastReportEntry* unitlab_native_client_session_append_last_report_entry(UnitLabNativeClientSessionState* session, const char* data_reference, int dataset_match, size_t inclusion_index);
int unitlab_native_client_session_data_set_member_exists(const UnitLabNativeClientSessionState* session, const char* reference);
int unitlab_native_client_session_data_set_index_by_reference(const UnitLabNativeClientSessionState* session, const char* reference, size_t* data_set_index);
int unitlab_native_client_session_data_set_contains_member(const UnitLabNativeClientSessionState* session, const char* data_set_reference, const char* member_reference);
const char* unitlab_native_client_session_data_set_member_at(const UnitLabNativeClientSessionState* session, const char* data_set_reference, size_t member_index);
UnitLabNativeDiscoveredDataSet* unitlab_native_client_session_append_data_set(UnitLabNativeClientSessionState* session, const char* data_set_reference);
int unitlab_native_client_session_append_data_set_member(UnitLabNativeClientSessionState* session, UnitLabNativeDiscoveredDataSet* data_set, const char* member_reference);
UnitLabNativeDiscoveredRcb* unitlab_native_client_session_append_discovered_rcb(UnitLabNativeClientSessionState* session, const char* domain, const char* item);
const UnitLabNativeDiscoveredRcb* unitlab_native_client_session_discovered_rcb_at(const UnitLabNativeClientSessionState* session, size_t index);

#endif
