#ifndef UNITLAB_IEC61850_UNITLAB_SUBSCRIPTION_PLANNER_H
#define UNITLAB_IEC61850_UNITLAB_SUBSCRIPTION_PLANNER_H

#include <stddef.h>

#include "app/planner/unitlab_verification_target.h"
#include "model/model_plan.h"

typedef enum UnitLabIedSubscriptionSourceKind {
    UNITLAB_IED_SUBSCRIPTION_SOURCE_NOT_FOUND = 0,
    UNITLAB_IED_SUBSCRIPTION_SOURCE_SCD,
    UNITLAB_IED_SUBSCRIPTION_SOURCE_DISCOVERY,
    UNITLAB_IED_SUBSCRIPTION_SOURCE_FALLBACK
} UnitLabIedSubscriptionSourceKind;

typedef struct UnitLabIedSubscriptionPlanGroup {
    size_t report_control_index;
    char ied_name[128];
    char access_point_name[128];
    char endpoint_id[160];
    char endpoint_label[192];
    char report_control_name[128];
    char report_kind[32];
    char rpt_id[256];
    char data_set_reference[256];
    size_t first_assignment_index;
    size_t assignment_count;
} UnitLabIedSubscriptionPlanGroup;

typedef struct UnitLabIedSubscriptionPlanAssignment {
    size_t target_index;
    size_t group_index;
    size_t matched_report_control_index;
    UnitLabIedSubscriptionSourceKind source_kind;
    char source_label[32];
    char reason[128];
} UnitLabIedSubscriptionPlanAssignment;

typedef struct UnitLabIedSubscriptionPlan {
    size_t group_count;
    UnitLabIedSubscriptionPlanGroup* groups;
    size_t assignment_count;
    UnitLabIedSubscriptionPlanAssignment* assignments;
} UnitLabIedSubscriptionPlan;

int unitlab_build_ied_subscription_plan(
    const UnitLabIedModelPlan* plan,
    const UnitLabIedVerificationTarget* targets,
    size_t target_count,
    UnitLabIedSubscriptionPlan* subscription_plan,
    char* error,
    size_t error_size);

void unitlab_free_ied_subscription_plan(UnitLabIedSubscriptionPlan* subscription_plan);

#endif
