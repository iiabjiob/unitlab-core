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

typedef enum UnitLabIedFixtureValueKind {
    UNITLAB_IED_FIXTURE_VALUE_UNKNOWN = 0,
    UNITLAB_IED_FIXTURE_VALUE_NULL,
    UNITLAB_IED_FIXTURE_VALUE_BOOLEAN,
    UNITLAB_IED_FIXTURE_VALUE_INTEGER,
    UNITLAB_IED_FIXTURE_VALUE_REAL,
    UNITLAB_IED_FIXTURE_VALUE_STRING,
} UnitLabIedFixtureValueKind;

typedef struct UnitLabIedFixtureSignal {
    size_t data_set_index;
    char reference[256];
    char kind[32];
    char component[128];
    char fc[32];
    UnitLabIedFixtureValueKind initial_value_kind;
    char initial_value[128];
} UnitLabIedFixtureSignal;

typedef struct UnitLabIedFixtureDataSet {
    char reference[256];
    size_t signal_count;
    UnitLabIedFixtureSignal* signals;
} UnitLabIedFixtureDataSet;

typedef struct UnitLabIedFixtureOptionalBool {
    int known;
    int value;
} UnitLabIedFixtureOptionalBool;

typedef struct UnitLabIedFixtureTriggerOptions {
    UnitLabIedFixtureOptionalBool data_change;
    UnitLabIedFixtureOptionalBool quality_change;
    UnitLabIedFixtureOptionalBool data_update;
    UnitLabIedFixtureOptionalBool periodic;
    UnitLabIedFixtureOptionalBool general_interrogation;
} UnitLabIedFixtureTriggerOptions;

typedef struct UnitLabIedFixtureOptionalFields {
    UnitLabIedFixtureOptionalBool sequence_number;
    UnitLabIedFixtureOptionalBool timestamp;
    UnitLabIedFixtureOptionalBool reason_code;
    UnitLabIedFixtureOptionalBool data_set_name;
    UnitLabIedFixtureOptionalBool data_reference;
    UnitLabIedFixtureOptionalBool entry_id;
    UnitLabIedFixtureOptionalBool config_revision;
    UnitLabIedFixtureOptionalBool buffer_overflow;
} UnitLabIedFixtureOptionalFields;

typedef struct UnitLabIedFixtureReport {
    char key[256];
    char logical_device_inst[128];
    char logical_node_name[128];
    char report_control_name[128];
    char report_kind[32];
    char rpt_id[256];
    char data_set_ref[256];
    char conf_rev[64];
    int indexed_known;
    int indexed;
    int buffer_time_ms_known;
    int buffer_time_ms;
    int integrity_period_ms_known;
    int integrity_period_ms;
    UnitLabIedFixtureTriggerOptions trigger_options;
    UnitLabIedFixtureOptionalFields optional_fields;
} UnitLabIedFixtureReport;

typedef struct UnitLabIedFixtureModel {
    size_t device_count;
    char ied_name[128];
    char access_point_name[128];
    size_t data_set_count;
    UnitLabIedFixtureDataSet* data_sets;
    size_t report_count;
    UnitLabIedFixtureReport* reports;
    size_t signal_count;
} UnitLabIedFixtureModel;

int unitlab_parse_ied_fixture_summary(
    const char* fixture_text,
    const char* ied_name,
    UnitLabIedFixtureSummary* summary,
    char* error,
    size_t error_size);

int unitlab_parse_ied_fixture_model(
    const char* fixture_text,
    const char* ied_name,
    UnitLabIedFixtureModel* model,
    char* error,
    size_t error_size);

void unitlab_free_ied_fixture_model(UnitLabIedFixtureModel* model);

#endif
