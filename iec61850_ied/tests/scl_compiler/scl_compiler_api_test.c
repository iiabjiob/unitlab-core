#include "scl_compiler/unitlab_scl_compiler.h"

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

static int test_compile_selects_ied_through_c_api(void)
{
    const char* scl =
        "<?xml version=\"1.0\"?>"
        "<scl:SCL xmlns:scl=\"http://www.iec.ch/61850/2003/SCL\">"
        "<scl:IED name=\"IED1\"><scl:AccessPoint name=\"AP1\" /></scl:IED>"
        "<scl:IED name=\"IED2\"><scl:AccessPoint name=\"AP1\" /></scl:IED>"
        "</scl:SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "IED2", &result, error, sizeof(error)) == 1,
        "SCL compile should succeed through C API");
    passed &= expect_true(result != NULL, "SCL compile result should be allocated");
    passed &= expect_string(unitlab_scl_compile_selected_ied_name(result), "IED2", "selected IED");
    passed &= expect_true(unitlab_scl_compile_source_size(result) == strlen(scl), "source size should be preserved");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 1U, "pending compiler warning should be present");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "warning", "diagnostic severity");
    passed &= expect_string(diagnostic.code, "SCL_COMPILER_MODEL_PLAN_PENDING", "diagnostic code");

    unitlab_scl_compile_result_free(result);
    return passed;
}

static int test_compile_reports_invalid_selected_ied(void)
{
    const char* scl = "<SCL><IED name=\"IED1\" /></SCL>";
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(scl, strlen(scl), "MISSING", &result, error, sizeof(error)) == 1,
        "SCL compile should return diagnostics for missing selected IED");
    passed &= expect_true(unitlab_scl_compile_diagnostic_count(result) == 2U, "missing IED error plus pending warning");

    UnitLabSclCompileDiagnostic diagnostic;
    passed &= expect_true(unitlab_scl_compile_diagnostic_at(result, 0U, &diagnostic) == 1, "error diagnostic should be readable");
    passed &= expect_string(diagnostic.severity, "error", "missing IED severity");
    passed &= expect_string(diagnostic.code, "SCL_SELECTED_IED_MISSING", "missing IED code");

    unitlab_scl_compile_result_free(result);
    return passed;
}

static int test_compile_rejects_empty_input(void)
{
    UnitLabSclCompileResult* result = NULL;
    char error[128];
    int passed = 1;

    passed &= expect_true(unitlab_scl_compile_from_memory(NULL, 0U, NULL, &result, error, sizeof(error)) == 0,
        "empty SCL input should fail before allocating result");
    passed &= expect_true(result == NULL, "empty SCL input should not allocate result");
    passed &= expect_string(error, "SCL XML input is required", "empty SCL error");
    return passed;
}

int main(void)
{
    int passed = 1;
    passed &= test_compile_selects_ied_through_c_api();
    passed &= test_compile_reports_invalid_selected_ied();
    passed &= test_compile_rejects_empty_input();
    return passed ? 0 : 1;
}
