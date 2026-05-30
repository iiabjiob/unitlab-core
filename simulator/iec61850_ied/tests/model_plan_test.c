#include "model_plan.h"

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

static int starts_with(const char* value, const char* prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
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

static int test_model_plan_builds_blueprint(void)
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
            .data_set_index = 1U,
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
    passed &= expect_true(ok, "model plan should build");
    if (ok) {
        passed &= expect_true(plan.logical_device_count == 1U, "one logical device");
        passed &= expect_true(plan.logical_node_count == 3U, "three logical nodes");
        passed &= expect_true(plan.data_set_count == 1U, "one DataSet");
        passed &= expect_true(plan.report_count == 1U, "one ReportControl");
        passed &= expect_true(plan.signal_count == 2U, "two signals");
        passed &= expect_string(plan.data_sets[0].logical_device_inst, "LD0", "DataSet LD");
        passed &= expect_string(plan.data_sets[0].logical_node_name, "LLN0", "DataSet LN");
        passed &= expect_string(plan.data_sets[0].name, "dsEvents", "DataSet name");
        passed &= expect_string(plan.reports[0].name, "brcbEvents", "ReportControl name");
        passed &= expect_string(plan.reports[0].report_kind, "buffered", "ReportControl kind");
        passed &= expect_true(plan.reports[0].is_buffered == 1, "ReportControl buffered flag");
        passed &= expect_string(plan.reports[0].rpt_id, "events", "ReportControl rptID");
        passed &= expect_true(plan.reports[0].conf_rev_known == 1, "ReportControl ConfRev known");
        passed &= expect_true(plan.reports[0].conf_rev == 1U, "ReportControl ConfRev");
        passed &= expect_true(plan.reports[0].indexed_known == 1, "ReportControl indexed known");
        passed &= expect_true(plan.reports[0].indexed == 0, "ReportControl indexed value");
        passed &= expect_true(plan.reports[0].buffer_time_ms_known == 1, "ReportControl BufTm known");
        passed &= expect_true(plan.reports[0].buffer_time_ms == 100U, "ReportControl BufTm");
        passed &= expect_true(plan.reports[0].integrity_period_ms_known == 1, "ReportControl IntgPd known");
        passed &= expect_true(plan.reports[0].integrity_period_ms == 1000U, "ReportControl IntgPd");
        passed &= expect_true(plan.reports[0].trigger_options.data_change.known == 1, "ReportControl dchg known");
        passed &= expect_true(plan.reports[0].trigger_options.data_change.value == 1, "ReportControl dchg value");
        passed &= expect_true(plan.reports[0].optional_fields.data_reference.known == 1, "ReportControl dataRef known");
        passed &= expect_true(plan.reports[0].optional_fields.data_reference.value == 1, "ReportControl dataRef value");
        passed &= expect_string(plan.signals[0].logical_node_name, "XCBR1", "first signal LN");
        passed &= expect_string(plan.signals[0].kind, "FCDA", "first signal kind");
        passed &= expect_string(plan.signals[0].object_reference, "Pos.stVal", "first signal object reference");
        passed &= expect_string(plan.signals[0].data_object_name, "Pos", "first signal data object");
        passed &= expect_string(plan.signals[0].data_attribute_path, "stVal", "first signal data attribute");
        passed &= expect_string(plan.signals[0].fc, "ST", "first signal FC");
        passed &= expect_true(plan.signals[0].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "first signal value kind");
        passed &= expect_string(plan.signals[0].initial_value, "0", "first signal initial value");
        passed &= expect_string(plan.signals[1].kind, "FCD", "second signal kind");
        passed &= expect_string(plan.signals[1].object_reference, "Ind1", "second signal FCD parent object reference");
        passed &= expect_string(plan.signals[1].data_object_name, "Ind1", "second signal data object");
        passed &= expect_string(plan.signals[1].data_attribute_path, "", "second signal data attribute");
        passed &= expect_true(plan.signals[1].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "second signal value kind");
    }
    unitlab_free_ied_model_plan(&plan);
    return passed;
}

static int test_missing_report_dataset_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.missing"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "missing report DataSet should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_REPORT_DATASET_NOT_FOUND"), "missing DataSet error code");
}

static int test_invalid_signal_reference_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid signal reference should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_REFERENCE_INVALID"), "invalid signal error code");
}

static int test_signal_fc_mismatch_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCD",
            .fc = "MX",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "signal FC mismatch should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_FC_MISMATCH"), "FC mismatch error code");
}

static int test_invalid_signal_kind_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "BAD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid signal kind should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_KIND_INVALID"), "invalid signal kind error code");
}

static int test_invalid_report_kind_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    snprintf(reports[0].report_kind, sizeof(reports[0].report_kind), "%s", "unknown");
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid report kind should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_REPORT_KIND_INVALID"), "invalid report kind error code");
}

static int test_invalid_report_conf_rev_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    snprintf(reports[0].conf_rev, sizeof(reports[0].conf_rev), "%s", "bad");
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "invalid report ConfRev should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_REPORT_CONFREV_INVALID"), "invalid ConfRev error code");
}

static int test_unknown_initial_value_kind_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCDA",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_UNKNOWN,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "IED1/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("IED1/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "unknown value kind should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_SIGNAL_VALUE_KIND_UNKNOWN"), "unknown value kind error code");
}

static int test_dataset_context_mismatch_fails(void)
{
    UnitLabIedFixtureSignal signals[1] = {
        {
            .data_set_index = 0U,
            .reference = "LD0/XCBR1.Pos.stVal[ST]",
            .kind = "FCD",
            .fc = "ST",
            .initial_value_kind = UNITLAB_IED_FIXTURE_VALUE_INTEGER,
            .initial_value = "0",
        },
    };
    UnitLabIedFixtureDataSet data_sets[1] = {
        {
            .reference = "OTHER/AP1/LD0/LLN0.dsEvents",
            .signal_count = 1U,
            .signals = signals,
        },
    };
    UnitLabIedFixtureReport reports[1] = {
        report_for_data_set("OTHER/AP1/LD0/LLN0.dsEvents"),
    };
    UnitLabIedFixtureModel fixture = fixture_for(data_sets, 1U, reports, 1U, 1U);
    UnitLabIedModelPlan plan;
    char error[256];
    int ok = unitlab_build_ied_model_plan(&fixture, &plan, error, sizeof(error));
    unitlab_free_ied_model_plan(&plan);
    return expect_true(!ok, "DataSet IED/AP context mismatch should fail")
        && expect_true(starts_with(error, "MODEL_PLAN_DATASET_CONTEXT_MISMATCH"), "DataSet context mismatch error code");
}

int main(void)
{
    int passed = 1;
    passed &= test_model_plan_builds_blueprint();
    passed &= test_missing_report_dataset_fails();
    passed &= test_invalid_signal_reference_fails();
    passed &= test_signal_fc_mismatch_fails();
    passed &= test_invalid_signal_kind_fails();
    passed &= test_invalid_report_kind_fails();
    passed &= test_invalid_report_conf_rev_fails();
    passed &= test_unknown_initial_value_kind_fails();
    passed &= test_dataset_context_mismatch_fails();
    return passed ? 0 : 1;
}
