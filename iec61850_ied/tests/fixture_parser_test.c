#include "fixture/fixture_parser.h"

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

static const char* fixture_with_initial_values(void)
{
    return "{"
        "\"schema\":\"unitlab.iec61850.ied-simulator-fixture.v1\","
        "\"devices\":[{"
            "\"iedName\":\"IED1\","
            "\"accessPointName\":\"AP1\","
            "\"dataSets\":[{"
                "\"reference\":\"IED1/AP1/LD0/LLN0.dsEvents\","
                "\"members\":["
                    "{\"dataSetIndex\":0,\"reference\":\"LD0/GGIO1.Ind1.stVal[ST]\",\"kind\":\"FCDA\",\"component\":\"phaseA\",\"fc\":\"ST\",\"initialValue\":true},"
                    "{\"dataSetIndex\":1,\"reference\":\"LD0/GGIO1.Ind2.stVal[ST]\",\"kind\":\"FCDA\",\"fc\":\"ST\",\"initialValue\":42},"
                    "{\"dataSetIndex\":2,\"reference\":\"LD0/MMXU1.A.phsA.cVal.mag.f[MX]\",\"kind\":\"FCDA\",\"fc\":\"MX\",\"initialValue\":3.14},"
                    "{\"dataSetIndex\":3,\"reference\":\"LD0/GGIO1.NamPlt.vendor[DC]\",\"kind\":\"FCDA\",\"fc\":\"DC\",\"initialValue\":\"UnitLab\"},"
                    "{\"dataSetIndex\":4,\"reference\":\"LD0/GGIO1.Ind3[ST]\",\"kind\":\"FCD\",\"fc\":\"ST\",\"initialValue\":null}"
                "]"
            "}],"
            "\"reports\":[{"
                "\"key\":\"IED1/AP1/LD0/LLN0/brcbEvents/buffered\","
                "\"logicalDeviceInst\":\"LD0\","
                "\"logicalNodeName\":\"LLN0\","
                "\"reportControlName\":\"brcbEvents\","
                "\"reportKind\":\"buffered\","
                "\"rptId\":\"events\","
                "\"dataSetRef\":\"IED1/AP1/LD0/LLN0.dsEvents\","
                "\"confRev\":\"1\","
                "\"indexed\":true,"
                "\"bufferTimeMs\":0,"
                "\"integrityPeriodMs\":0,"
                "\"triggerOptions\":{"
                    "\"dataChange\":true,"
                    "\"qualityChange\":true,"
                    "\"dataUpdate\":false,"
                    "\"periodic\":false,"
                    "\"generalInterrogation\":true"
                "},"
                "\"optionalFields\":{"
                    "\"sequenceNumber\":true,"
                    "\"timestamp\":true,"
                    "\"reasonCode\":true,"
                    "\"dataSetName\":true,"
                    "\"dataReference\":true,"
                    "\"entryId\":true,"
                    "\"configRevision\":true,"
                    "\"bufferOverflow\":true"
                "}"
            "}]"
        "}]"
    "}";
}

static const char* fixture_with_invalid_initial_value(void)
{
    return "{"
        "\"schema\":\"unitlab.iec61850.ied-simulator-fixture.v1\","
        "\"devices\":[{"
            "\"iedName\":\"IED1\","
            "\"accessPointName\":\"AP1\","
            "\"dataSets\":[{"
                "\"reference\":\"IED1/AP1/LD0/LLN0.dsEvents\","
                "\"members\":["
                    "{\"dataSetIndex\":0,\"reference\":\"LD0/GGIO1.Ind1.stVal[ST]\",\"kind\":\"FCDA\",\"fc\":\"ST\",\"initialValue\":badToken}"
                "]"
            "}],"
            "\"reports\":[{"
                "\"key\":\"IED1/AP1/LD0/LLN0/brcbEvents/buffered\","
                "\"logicalDeviceInst\":\"LD0\","
                "\"logicalNodeName\":\"LLN0\","
                "\"reportControlName\":\"brcbEvents\","
                "\"reportKind\":\"buffered\","
                "\"rptId\":\"events\","
                "\"dataSetRef\":\"IED1/AP1/LD0/LLN0.dsEvents\","
                "\"confRev\":\"1\","
                "\"indexed\":true,"
                "\"bufferTimeMs\":0,"
                "\"integrityPeriodMs\":0,"
                "\"triggerOptions\":{"
                    "\"dataChange\":true,"
                    "\"qualityChange\":true,"
                    "\"dataUpdate\":false,"
                    "\"periodic\":false,"
                    "\"generalInterrogation\":true"
                "},"
                "\"optionalFields\":{"
                    "\"sequenceNumber\":true,"
                    "\"timestamp\":true,"
                    "\"reasonCode\":true,"
                    "\"dataSetName\":true,"
                    "\"dataReference\":true,"
                    "\"entryId\":true,"
                    "\"configRevision\":true,"
                    "\"bufferOverflow\":true"
                "}"
            "}]"
        "}]"
    "}";
}

static int test_initial_value_kinds_are_preserved(void)
{
    UnitLabIedFixtureModel model;
    char error[256];
    int parsed = unitlab_parse_ied_fixture_model(fixture_with_initial_values(), "IED1", &model, error, sizeof(error));
    int passed = expect_true(parsed, "fixture should parse");
    if (parsed) {
        UnitLabIedFixtureSignal* signals = model.data_sets[0].signals;
        passed &= expect_string(signals[0].component, "phaseA", "component value");
        passed &= expect_true(signals[0].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_BOOLEAN, "boolean value kind");
        passed &= expect_string(signals[0].initial_value, "true", "boolean value");
        passed &= expect_true(signals[1].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_INTEGER, "integer value kind");
        passed &= expect_string(signals[1].initial_value, "42", "integer value");
        passed &= expect_true(signals[2].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_REAL, "real value kind");
        passed &= expect_string(signals[2].initial_value, "3.14", "real value");
        passed &= expect_true(signals[3].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_STRING, "string value kind");
        passed &= expect_string(signals[3].initial_value, "UnitLab", "string value");
        passed &= expect_true(signals[4].initial_value_kind == UNITLAB_IED_FIXTURE_VALUE_NULL, "null value kind");
        passed &= expect_string(signals[4].initial_value, "null", "null value");
    }
    unitlab_free_ied_fixture_model(&model);
    return passed;
}

static int test_invalid_initial_value_fails(void)
{
    UnitLabIedFixtureModel model;
    char error[256];
    int parsed = unitlab_parse_ied_fixture_model(fixture_with_invalid_initial_value(), "IED1", &model, error, sizeof(error));
    unitlab_free_ied_fixture_model(&model);
    return expect_true(!parsed, "invalid initialValue token should fail")
        && expect_true(starts_with(error, "FIXTURE_SIGNAL_INITIAL_VALUE_INVALID"), "invalid initialValue error code");
}

int main(void)
{
    int passed = 1;
    passed &= test_initial_value_kinds_are_preserved();
    passed &= test_invalid_initial_value_fails();
    return passed ? 0 : 1;
}
