#include "app/planner/unitlab_subscription_planner.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
    }
    return condition;
}

static int expect_string(const char* actual, const char* expected, const char* message)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s: expected \"%s\", got \"%s\"\n", message, expected, actual);
        return 0;
    }
    return 1;
}

static UnitLabIedFixtureReport report_for_data_set(const char* data_set_ref)
{
    UnitLabIedFixtureReport report = {
        .key = "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
        .logical_device_inst = "LD0",
        .logical_node_name = "LLN0",
        .report_control_name = "brcbEvents",
        .report_kind = "buffered",
        .rpt_id = "events",
        .conf_rev = "1",
        .indexed_known = 1,
        .indexed = 0,
        .buffer_time_ms_known = 1,
        .buffer_time_ms = 100,
        .integrity_period_ms_known = 1,
        .integrity_period_ms = 1000,
    };
    report.trigger_options.data_change = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.trigger_options.general_interrogation = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    snprintf(report.data_set_ref, sizeof(report.data_set_ref), "%s", data_set_ref);
    return report;
}

static UnitLabIedFixtureModel fixture_for(
    UnitLabIedFixtureDataSet* data_sets,
    size_t data_set_count,
    UnitLabIedFixtureReport* reports,
    size_t report_count,
    size_t signal_count)
{
    UnitLabIedFixtureModel fixture = {
        .device_count = 1U,
        .ied_name = "IED1",
        .access_point_name = "AP1",
        .data_set_count = data_set_count,
        .data_sets = data_sets,
        .report_count = report_count,
        .reports = reports,
        .signal_count = signal_count,
    };
    return fixture;
}

static int test_builds_subscription_plan(void)
{
    UnitLabIedFixtureSignal signals[2] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
        {
            .data_set_index = 0U,
            .reference = "LD0/PGGIO1.Ind1[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "1",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 2U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 2U);
    UnitLabIedModelPlan plan;
    char error[256];
    int passed = 1;
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    UnitLabIedVerificationTarget* targets = NULL;
    size_t target_count = 0U;
    const size_t selected_signal_indexes[2] = { 0U, 1U };
    UnitLabIedSubscriptionPlan subscription_plan;

    passed &= expect_true(ok, "model plan should build");
    if (!ok) {
        return 0;
    }

    ok = unitlab_collect_ied_verification_targets(
        &fixture,
        &plan,
        "mms:IED1@127.0.0.1:102",
        selected_signal_indexes,
        2U,
        2500U,
        500U,
        &targets,
        &target_count,
        error,
        sizeof(error));
    passed &= expect_true(ok, "verification targets should collect");
    if (!ok) {
        unitlab_free_ied_model_plan(&plan);
        return 0;
    }

    snprintf(targets[1].data_set_reference, sizeof(targets[1].data_set_reference), "%s", "IED1/AP1/LD0/LLN0.other");

    UnitLabIedVerificationTarget not_found = targets[0];
    snprintf(not_found.logical_node_name, sizeof(not_found.logical_node_name), "%s", "LLN9");
    snprintf(not_found.data_set_reference, sizeof(not_found.data_set_reference), "%s", "IED1/AP1/LD0/LLN9.dsMissing");

    UnitLabIedVerificationTarget inputs[3] = {
        targets[0],
        targets[1],
        not_found,
    };

    ok = unitlab_build_ied_subscription_plan(&plan, inputs, 3U, &subscription_plan, error, sizeof(error));
    passed &= expect_true(ok, "subscription plan should build");
    if (ok) {
        passed &= expect_true(subscription_plan.group_count == 1U, "one subscription group");
        passed &= expect_true(subscription_plan.assignment_count == 3U, "three assignments");
        if (subscription_plan.group_count == 1U) {
            passed &= expect_string(subscription_plan.groups[0].report_control_name, "brcbEvents", "group report control name");
            passed &= expect_string(subscription_plan.groups[0].rpt_id, "events", "group rptID");
            passed &= expect_string(subscription_plan.groups[0].data_set_reference, "IED1/AP1/LD0/LLN0.dsEvents", "group data set reference");
            passed &= expect_true(subscription_plan.groups[0].assignment_count == 2U, "group assignment count");
            passed &= expect_true(subscription_plan.groups[0].first_assignment_index == 0U, "group first assignment index");
        }
        passed &= expect_true(subscription_plan.assignments[0].group_index == 0U, "first assignment grouped");
        passed &= expect_true(subscription_plan.assignments[0].source_kind == UNITLAB_IED_SUBSCRIPTION_SOURCE_SCD, "first assignment source kind");
        passed &= expect_string(subscription_plan.assignments[0].source_label, "from SCD", "first assignment source label");
        passed &= expect_string(subscription_plan.assignments[0].reason, "exact dataset match", "first assignment reason");

        passed &= expect_true(subscription_plan.assignments[1].group_index == 0U, "second assignment grouped");
        passed &= expect_true(subscription_plan.assignments[1].source_kind == UNITLAB_IED_SUBSCRIPTION_SOURCE_FALLBACK, "second assignment source kind");
        passed &= expect_string(subscription_plan.assignments[1].source_label, "fallback", "second assignment source label");
        passed &= expect_string(subscription_plan.assignments[1].reason, "single report control fallback", "second assignment reason");

        passed &= expect_true(subscription_plan.assignments[2].group_index == (size_t)-1, "third assignment uncovered");
        passed &= expect_true(subscription_plan.assignments[2].source_kind == UNITLAB_IED_SUBSCRIPTION_SOURCE_NOT_FOUND, "third assignment source kind");
        passed &= expect_string(subscription_plan.assignments[2].source_label, "not found", "third assignment source label");
        passed &= expect_string(subscription_plan.assignments[2].reason, "no matching report control", "third assignment reason");
    }

    unitlab_free_ied_subscription_plan(&subscription_plan);
    unitlab_free_ied_verification_targets(targets);
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

int main(void)
{
    return test_builds_subscription_plan() ? 0 : 1;
}
