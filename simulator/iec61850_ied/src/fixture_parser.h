#ifndef UNITLAB_IEC61850_IED_FIXTURE_PARSER_H
#define UNITLAB_IEC61850_IED_FIXTURE_PARSER_H

#include <stddef.h>

#define UNITLAB_IED_SIM_SCHEMA "unitlab.iec61850.ied-simulator-fixture.v1"

typedef struct UnitLabIedFixtureSummary {
    size_t device_count;
    size_t data_set_count;
    size_t report_count;
    size_t signal_count;
    char access_point_name[128];
} UnitLabIedFixtureSummary;

int unitlab_parse_ied_fixture_summary(
    const char* fixture_text,
    const char* ied_name,
    UnitLabIedFixtureSummary* summary,
    char* error,
    size_t error_size);

#endif
