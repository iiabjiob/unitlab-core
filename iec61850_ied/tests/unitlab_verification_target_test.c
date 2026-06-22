#include "app/planner/unitlab_verification_target.h"

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
    report.trigger_options.quality_change = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.trigger_options.data_update = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.trigger_options.periodic = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.trigger_options.general_interrogation = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.sequence_number = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.timestamp = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.reason_code = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.data_set_name = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.data_reference = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 1 };
    report.optional_fields.buffer_overflow = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.entry_id = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
    report.optional_fields.config_revision = (UnitLabIedFixtureOptionalBool){ .known = 1, .value = 0 };
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

static int test_collects_verification_targets(void)
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
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    int passed = 1;
    const size_t selected_signal_indexes[2] = { 1U, 0U };
    UnitLabIedVerificationTarget* targets = NULL;
    size_t count = 0U;

    passed &= expect_true(ok, "model plan should build for verification targets");
    if (ok) {
        ok = unitlab_collect_ied_verification_targets(
            &fixture,
            &plan,
            "mms:IED1@127.0.0.1:102",
            selected_signal_indexes,
            2U,
            2500U,
            500U,
            &targets,
            &count,
            error,
            sizeof(error));
        passed &= expect_true(ok, "verification targets should collect");
        if (ok) {
            passed &= expect_true(count == 2U, "two verification targets");
            if (count == 2U) {
                passed &= expect_true(targets[0].source_signal_index == 0U, "first target source signal index");
                passed &= expect_true(targets[0].source_data_set_index == 0U, "first target source dataset index");
                passed &= expect_true(targets[0].source_member_index == plan.signals[0].member_index, "first target source member index");
                passed &= expect_string(targets[0].ied_name, "IED1", "first target IED name");
                passed &= expect_string(targets[0].access_point_name, "AP1", "first target access point");
                passed &= expect_string(targets[0].endpoint_id, "mms:IED1@127.0.0.1:102", "first target endpoint id");
                passed &= expect_string(targets[0].endpoint_label, "mms:IED1@127.0.0.1:102", "first target endpoint label");
                passed &= expect_string(targets[0].signal_reference, "LD0/XCBR1.Pos.stVal[ST]", "first target signal reference");
                passed &= expect_string(targets[0].signal_path, "IED1LD0/XCBR1.Pos.stVal", "first target signal path");
                passed &= expect_string(targets[0].logical_device_inst, "IED1LD0", "first target logical device");
                passed &= expect_string(targets[0].logical_node_name, "XCBR1", "first target logical node");
                passed &= expect_string(targets[0].data_set_reference, "IED1/AP1/LD0/LLN0.dsEvents", "first target dataset reference");
                passed &= expect_string(targets[0].expected_feedback_path, "IED1LD0/XCBR1$ST$Pos$stVal", "first target expected feedback path");
                passed &= expect_true(targets[0].timeout_ms == 2500U, "first target timeout");
                passed &= expect_true(targets[0].window_ms == 500U, "first target window");

                passed &= expect_true(targets[1].source_signal_index == 1U, "second target source signal index");
                passed &= expect_true(targets[1].source_data_set_index == 0U, "second target source dataset index");
                passed &= expect_true(targets[1].source_member_index == plan.signals[1].member_index, "second target source member index");
                passed &= expect_string(targets[1].signal_reference, "LD0/PGGIO1.Ind1[ST]", "second target signal reference");
                passed &= expect_string(targets[1].signal_path, "IED1LD0/PGGIO1.Ind1", "second target signal path");
                passed &= expect_string(targets[1].expected_feedback_path, "IED1LD0/PGGIO1$ST$Ind1", "second target expected feedback path");
                passed &= expect_string(targets[1].data_set_reference, "IED1/AP1/LD0/LLN0.dsEvents", "second target dataset reference");
            }
        }
    }

    unitlab_free_ied_verification_targets(targets);
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

int main(void)
{
    return test_collects_verification_targets() ? 0 : 1;
}
