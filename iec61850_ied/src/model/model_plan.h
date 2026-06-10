#ifndef UNITLAB_IEC61850_IED_MODEL_PLAN_H
#define UNITLAB_IEC61850_IED_MODEL_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "fixture/fixture_parser.h"

#define UNITLAB_IED_MODEL_TRG_OPT_DATA_CHANGED 1U
#define UNITLAB_IED_MODEL_TRG_OPT_QUALITY_CHANGED 2U
#define UNITLAB_IED_MODEL_TRG_OPT_DATA_UPDATE 4U
#define UNITLAB_IED_MODEL_TRG_OPT_INTEGRITY 8U
#define UNITLAB_IED_MODEL_TRG_OPT_GI 16U

#define UNITLAB_IED_MODEL_RPT_OPT_SEQ_NUM 1U
#define UNITLAB_IED_MODEL_RPT_OPT_TIME_STAMP 2U
#define UNITLAB_IED_MODEL_RPT_OPT_REASON_FOR_INCLUSION 4U
#define UNITLAB_IED_MODEL_RPT_OPT_DATA_SET 8U
#define UNITLAB_IED_MODEL_RPT_OPT_DATA_REFERENCE 16U
#define UNITLAB_IED_MODEL_RPT_OPT_BUFFER_OVERFLOW 32U
#define UNITLAB_IED_MODEL_RPT_OPT_ENTRY_ID 64U
#define UNITLAB_IED_MODEL_RPT_OPT_CONF_REV 128U

#define UNITLAB_IED_MODEL_MAX_ENUM_VALUES 32U

typedef struct UnitLabIedModelEnumValue {
    int32_t ord;
    char text[128];
} UnitLabIedModelEnumValue;

typedef struct UnitLabIedModelLogicalDevice {
    char inst[128];
} UnitLabIedModelLogicalDevice;

typedef struct UnitLabIedModelLogicalNode {
    char logical_device_inst[128];
    char name[128];
} UnitLabIedModelLogicalNode;

typedef struct UnitLabIedModelDataSet {
    char reference[256];
    char logical_device_inst[128];
    char logical_node_name[128];
    char name[128];
    size_t first_signal_index;
    size_t member_count;
} UnitLabIedModelDataSet;

typedef struct UnitLabIedModelReportControl {
    char key[256];
    char logical_device_inst[128];
    char logical_node_name[128];
    char name[128];
    char report_kind[32];
    int is_buffered;
    char rpt_id[256];
    char data_set_ref[256];
    size_t data_set_index;
    int conf_rev_known;
    uint32_t conf_rev;
    int indexed_known;
    int indexed;
    int buffer_time_ms_known;
    uint32_t buffer_time_ms;
    int integrity_period_ms_known;
    uint32_t integrity_period_ms;
    UnitLabIedFixtureTriggerOptions trigger_options;
    UnitLabIedFixtureOptionalFields optional_fields;
    uint8_t trigger_options_mask;
    uint8_t optional_fields_mask;
} UnitLabIedModelReportControl;

typedef struct UnitLabIedModelSignal {
    char reference[256];
    char kind[32];
    size_t data_set_index;
    size_t member_index;
    char logical_device_inst[128];
    char logical_node_name[128];
    char data_object_name[128];
    char data_attribute_path[128];
    char object_reference[192];
    char data_set_entry_variable[256];
    int data_set_entry_component_known;
    char data_set_entry_component[128];
    char fc[32];
    UnitLabIedFixtureValueKind initial_value_kind;
    char initial_value[128];
    int enum_type_known;
    char enum_type_id[128];
    size_t enum_value_count;
    UnitLabIedModelEnumValue enum_values[UNITLAB_IED_MODEL_MAX_ENUM_VALUES];
} UnitLabIedModelSignal;

typedef struct UnitLabIedModelNamespaceAttribute {
    char logical_device_inst[128];
    char logical_node_name[128];
    char data_object_name[128];
    char name[32];
    char object_reference[192];
    UnitLabIedFixtureValueKind initial_value_kind;
    char initial_value[128];
} UnitLabIedModelNamespaceAttribute;

typedef struct UnitLabIedModelPlan {
    size_t logical_device_count;
    UnitLabIedModelLogicalDevice* logical_devices;
    size_t logical_node_count;
    UnitLabIedModelLogicalNode* logical_nodes;
    size_t data_set_count;
    UnitLabIedModelDataSet* data_sets;
    size_t report_count;
    UnitLabIedModelReportControl* reports;
    size_t signal_count;
    UnitLabIedModelSignal* signals;
    size_t namespace_attribute_count;
    UnitLabIedModelNamespaceAttribute* namespace_attributes;
} UnitLabIedModelPlan;

int unitlab_build_ied_model_plan(
    const UnitLabIedFixtureModel* fixture,
    UnitLabIedModelPlan* plan,
    char* error,
    size_t error_size);

typedef enum UnitLabIedModelReportControlKind {
    UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_BUFFERED = 0,
    UNITLAB_IED_MODEL_REPORT_CONTROL_KIND_UNBUFFERED = 1
} UnitLabIedModelReportControlKind;

int unitlab_collect_ied_model_logical_devices(
    const UnitLabIedModelPlan* plan,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_logical_node_data_sets(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_logical_device_data_sets(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_vmd_named_variable_lists(
    const UnitLabIedModelPlan* plan,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_logical_device_variables(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_logical_node_variables(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_logical_node_namespace_attributes(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

int unitlab_collect_ied_model_logical_node_reports(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    UnitLabIedModelReportControlKind kind,
    char*** names,
    size_t* count,
    char* error,
    size_t error_size);

const UnitLabIedModelReportControl* unitlab_find_ied_model_report_control(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* report_name);

const UnitLabIedModelDataSet* unitlab_find_ied_model_data_set(
    const UnitLabIedModelPlan* plan,
    const char* logical_device_inst,
    const char* logical_node_name,
    const char* data_set_name);

const UnitLabIedModelNamespaceAttribute* unitlab_find_ied_model_namespace_attribute(
    const UnitLabIedModelPlan* plan,
    const char* object_reference);

void unitlab_free_ied_model_name_list(char** names, size_t count);
void unitlab_free_ied_model_plan(UnitLabIedModelPlan* plan);

#endif
