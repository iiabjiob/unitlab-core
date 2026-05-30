#ifndef UNITLAB_IEC61850_IED_MODEL_PLAN_H
#define UNITLAB_IEC61850_IED_MODEL_PLAN_H

#include <stddef.h>

#include "fixture_parser.h"

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
    char data_set_ref[256];
    size_t data_set_index;
} UnitLabIedModelReportControl;

typedef struct UnitLabIedModelSignal {
    char reference[256];
    size_t data_set_index;
    size_t member_index;
    char logical_device_inst[128];
    char logical_node_name[128];
    char object_reference[192];
    char fc[32];
    char initial_value[128];
} UnitLabIedModelSignal;

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
} UnitLabIedModelPlan;

int unitlab_build_ied_model_plan(
    const UnitLabIedFixtureModel* fixture,
    UnitLabIedModelPlan* plan,
    char* error,
    size_t error_size);

void unitlab_free_ied_model_plan(UnitLabIedModelPlan* plan);

#endif
