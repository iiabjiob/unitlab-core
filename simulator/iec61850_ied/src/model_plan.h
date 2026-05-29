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
    size_t member_count;
} UnitLabIedModelDataSet;

typedef struct UnitLabIedModelReportControl {
    char key[256];
    char data_set_ref[256];
    size_t data_set_index;
} UnitLabIedModelReportControl;

typedef struct UnitLabIedModelPlan {
    size_t logical_device_count;
    UnitLabIedModelLogicalDevice* logical_devices;
    size_t logical_node_count;
    UnitLabIedModelLogicalNode* logical_nodes;
    size_t data_set_count;
    UnitLabIedModelDataSet* data_sets;
    size_t report_count;
    UnitLabIedModelReportControl* reports;
} UnitLabIedModelPlan;

int unitlab_build_ied_model_plan(
    const UnitLabIedFixtureModel* fixture,
    UnitLabIedModelPlan* plan,
    char* error,
    size_t error_size);

void unitlab_free_ied_model_plan(UnitLabIedModelPlan* plan);

#endif
