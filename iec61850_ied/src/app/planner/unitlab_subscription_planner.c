#include "app/planner/unitlab_subscription_planner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char* error, size_t error_size, const char* message)
{
    if (error == NULL || error_size == 0U) {
        return;
    }
    snprintf(error, error_size, "%s", message);
}

static void copy_c_string(char* destination, size_t destination_size, const char* source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }
    if (source == NULL) {
        source = "";
    }
    snprintf(destination, destination_size, "%s", source);
}

static size_t find_exact_report_control(const UnitLabIedModelPlan* plan, const UnitLabIedVerificationTarget* target)
{
    if (plan == NULL || target == NULL || plan->reports == NULL) {
        return (size_t)-1;
    }
    for (size_t index = 0U; index < plan->report_count; index++) {
        const UnitLabIedModelReportControl* report = &plan->reports[index];
        if (
            strcmp(report->logical_device_inst, target->logical_device_inst) == 0
            && strcmp(report->logical_node_name, target->logical_node_name) == 0
            && strcmp(report->data_set_ref, target->data_set_reference) == 0
        ) {
            return index;
        }
    }
    return (size_t)-1;
}

static size_t find_single_fallback_report_control(const UnitLabIedModelPlan* plan, const UnitLabIedVerificationTarget* target)
{
    if (
        plan == NULL
        || target == NULL
        || plan->reports == NULL
        || plan->report_count != 1U
        || target->logical_device_inst[0] == '\0'
        || target->logical_node_name[0] == '\0'
    ) {
        return (size_t)-1;
    }
    const UnitLabIedModelReportControl* report = &plan->reports[0];
    if (
        strcmp(report->logical_device_inst, target->logical_device_inst) == 0
        && strcmp(report->logical_node_name, target->logical_node_name) == 0
    ) {
        return 0U;
    }
    return (size_t)-1;
}

static size_t find_group_index_by_report_control(const UnitLabIedSubscriptionPlan* subscription_plan, size_t report_control_index)
{
    for (size_t index = 0U; index < subscription_plan->group_count; index++) {
        if (subscription_plan->groups[index].report_control_index == report_control_index) {
            return index;
        }
    }
    return (size_t)-1;
}

static void fill_group_from_target(
    UnitLabIedSubscriptionPlanGroup* group,
    const UnitLabIedModelReportControl* report,
    const UnitLabIedVerificationTarget* target,
    size_t report_control_index)
{
    group->report_control_index = report_control_index;
    copy_c_string(group->ied_name, sizeof(group->ied_name), target->ied_name);
    copy_c_string(group->access_point_name, sizeof(group->access_point_name), target->access_point_name);
    copy_c_string(group->endpoint_id, sizeof(group->endpoint_id), target->endpoint_id);
    copy_c_string(group->endpoint_label, sizeof(group->endpoint_label), target->endpoint_label);
    copy_c_string(group->report_control_name, sizeof(group->report_control_name), report->name);
    copy_c_string(group->report_kind, sizeof(group->report_kind), report->report_kind);
    copy_c_string(group->rpt_id, sizeof(group->rpt_id), report->rpt_id);
    copy_c_string(group->data_set_reference, sizeof(group->data_set_reference), report->data_set_ref);
    group->first_assignment_index = 0U;
    group->assignment_count = 0U;
}

static void set_assignment(
    UnitLabIedSubscriptionPlanAssignment* assignment,
    size_t target_index,
    size_t group_index,
    size_t report_control_index,
    UnitLabIedSubscriptionSourceKind source_kind,
    const char* source_label,
    const char* reason)
{
    assignment->target_index = target_index;
    assignment->group_index = group_index;
    assignment->matched_report_control_index = report_control_index;
    assignment->source_kind = source_kind;
    copy_c_string(assignment->source_label, sizeof(assignment->source_label), source_label);
    copy_c_string(assignment->reason, sizeof(assignment->reason), reason);
}

static size_t append_group(
    UnitLabIedSubscriptionPlan* subscription_plan,
    size_t target_capacity,
    const UnitLabIedModelPlan* plan,
    const UnitLabIedVerificationTarget* target,
    size_t report_control_index)
{
    if (subscription_plan->group_count >= target_capacity) {
        return (size_t)-1;
    }
    size_t group_index = subscription_plan->group_count++;
    fill_group_from_target(&subscription_plan->groups[group_index], &plan->reports[report_control_index], target, report_control_index);
    return group_index;
}

int unitlab_build_ied_subscription_plan(
    const UnitLabIedModelPlan* plan,
    const UnitLabIedVerificationTarget* targets,
    size_t target_count,
    UnitLabIedSubscriptionPlan* subscription_plan,
    char* error,
    size_t error_size)
{
    if (subscription_plan == NULL) {
        return 0;
    }
    subscription_plan->group_count = 0U;
    subscription_plan->groups = NULL;
    subscription_plan->assignment_count = 0U;
    subscription_plan->assignments = NULL;

    if (plan == NULL || targets == NULL) {
        set_error(error, error_size, "UNITLAB_SUBSCRIPTION_PLAN_INVALID_INPUT: plan and targets are required.");
        return 0;
    }
    if (target_count == 0U) {
        return 1;
    }
    if (plan->report_count == 0U || plan->reports == NULL) {
        set_error(error, error_size, "UNITLAB_SUBSCRIPTION_PLAN_NO_REPORT_CONTROLS: no report controls available.");
        return 0;
    }

    UnitLabIedSubscriptionPlanGroup* groups = (UnitLabIedSubscriptionPlanGroup*)calloc(target_count, sizeof(UnitLabIedSubscriptionPlanGroup));
    UnitLabIedSubscriptionPlanAssignment* assignments = (UnitLabIedSubscriptionPlanAssignment*)calloc(target_count, sizeof(UnitLabIedSubscriptionPlanAssignment));
    if (groups == NULL || assignments == NULL) {
        free(groups);
        free(assignments);
        set_error(error, error_size, "OUT_OF_MEMORY: cannot build subscription plan.");
        return 0;
    }

    for (size_t target_index = 0U; target_index < target_count; target_index++) {
        const UnitLabIedVerificationTarget* target = &targets[target_index];
        size_t report_control_index = find_exact_report_control(plan, target);
        UnitLabIedSubscriptionSourceKind source_kind = UNITLAB_IED_SUBSCRIPTION_SOURCE_NOT_FOUND;
        const char* source_label = "not found";
        const char* reason = "no matching report control";
        size_t group_index = (size_t)-1;

        if (report_control_index != (size_t)-1) {
            source_kind = UNITLAB_IED_SUBSCRIPTION_SOURCE_SCD;
            source_label = "from SCD";
            reason = "exact dataset match";
        } else {
            report_control_index = find_single_fallback_report_control(plan, target);
            if (report_control_index != (size_t)-1) {
                source_kind = UNITLAB_IED_SUBSCRIPTION_SOURCE_FALLBACK;
                source_label = "fallback";
                reason = "single report control fallback";
            }
        }

        if (report_control_index != (size_t)-1) {
            group_index = find_group_index_by_report_control(subscription_plan, report_control_index);
            if (group_index == (size_t)-1) {
                group_index = append_group(subscription_plan, target_count, plan, target, report_control_index);
                if (group_index == (size_t)-1) {
                    free(groups);
                    free(assignments);
                    set_error(error, error_size, "OUT_OF_MEMORY: cannot append subscription group.");
                    return 0;
                }
            }
        }

        set_assignment(&assignments[target_index], target_index, group_index, report_control_index, source_kind, source_label, reason);
    }

    for (size_t group_index = 0U; group_index < subscription_plan->group_count; group_index++) {
        size_t first_assignment_index = (size_t)-1;
        size_t assignment_count = 0U;
        for (size_t target_index = 0U; target_index < target_count; target_index++) {
            if (assignments[target_index].group_index == group_index) {
                if (first_assignment_index == (size_t)-1) {
                    first_assignment_index = target_index;
                }
                assignment_count++;
            }
        }
        subscription_plan->groups[group_index].first_assignment_index = first_assignment_index == (size_t)-1 ? 0U : first_assignment_index;
        subscription_plan->groups[group_index].assignment_count = assignment_count;
    }

    subscription_plan->groups = groups;
    subscription_plan->assignments = assignments;
    subscription_plan->assignment_count = target_count;
    return 1;
}

void unitlab_free_ied_subscription_plan(UnitLabIedSubscriptionPlan* subscription_plan)
{
    if (subscription_plan == NULL) {
        return;
    }
    free(subscription_plan->groups);
    free(subscription_plan->assignments);
    subscription_plan->group_count = 0U;
    subscription_plan->groups = NULL;
    subscription_plan->assignment_count = 0U;
    subscription_plan->assignments = NULL;
}
