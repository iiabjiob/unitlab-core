#ifndef UNITLAB_IEC61850_UNITLAB_VERIFICATION_TARGET_H
#define UNITLAB_IEC61850_UNITLAB_VERIFICATION_TARGET_H

#include <stddef.h>
#include <stdint.h>

#include "fixture/fixture_parser.h"
#include "model/model_plan.h"

typedef struct UnitLabIedVerificationTarget {
    size_t source_signal_index;
    size_t source_data_set_index;
    size_t source_member_index;
    char ied_name[128];
    char access_point_name[128];
    char endpoint_id[160];
    char endpoint_label[192];
    char signal_reference[256];
    char signal_path[256];
    char logical_device_inst[128];
    char logical_node_name[128];
    char data_set_reference[256];
    char expected_feedback_path[256];
    uint32_t timeout_ms;
    uint32_t window_ms;
} UnitLabIedVerificationTarget;

int unitlab_collect_ied_verification_targets(
    const UnitLabIedFixtureModel* fixture,
    const UnitLabIedModelPlan* plan,
    const char* endpoint_id,
    const size_t* selected_signal_indexes,
    size_t selected_signal_count,
    uint32_t timeout_ms,
    uint32_t window_ms,
    UnitLabIedVerificationTarget** targets,
    size_t* count,
    char* error,
    size_t error_size);

void unitlab_free_ied_verification_targets(UnitLabIedVerificationTarget* targets);

#endif
