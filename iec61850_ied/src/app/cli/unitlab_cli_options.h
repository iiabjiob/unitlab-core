#ifndef UNITLAB_IEC61850_IED_APP_CLI_UNITLAB_CLI_OPTIONS_H
#define UNITLAB_IEC61850_IED_APP_CLI_UNITLAB_CLI_OPTIONS_H

/* Internal CLI options and context state. Keep this out of the public API. */

#include <stdint.h>

#include "server/native_wire_client.h"
#include "server/server_runtime.h"

typedef enum UnitLabCliSourceKind {
    UNITLAB_CLI_SOURCE_NONE = 0,
    UNITLAB_CLI_SOURCE_FIXTURE = 1,
    UNITLAB_CLI_SOURCE_SCL = 2,
    UNITLAB_CLI_SOURCE_MMS_CLIENT = 3
} UnitLabCliSourceKind;

typedef enum UnitLabCliCommandKind {
    UNITLAB_CLI_COMMAND_INVALID = 0,
    UNITLAB_CLI_COMMAND_FIXTURE_DRY_RUN = 1,
    UNITLAB_CLI_COMMAND_FIXTURE_SERVER = 2,
    UNITLAB_CLI_COMMAND_FIXTURE_SMOKE_START = 3,
    UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_SMOKE_START = 4,
    UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_WIRE_START = 5,
    UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_WIRE_CLIENT_START = 6,
    UNITLAB_CLI_COMMAND_FIXTURE_DISCOVER_PROBE = 7,
    UNITLAB_CLI_COMMAND_FIXTURE_METADATA_PROBE = 8,
    UNITLAB_CLI_COMMAND_FIXTURE_GI_PROBE = 9,
    UNITLAB_CLI_COMMAND_SCL_DRY_RUN = 10,
    UNITLAB_CLI_COMMAND_SCL_SERVER = 11,
    UNITLAB_CLI_COMMAND_SCL_SMOKE_START = 12,
    UNITLAB_CLI_COMMAND_SCL_NATIVE_SMOKE_START = 13,
    UNITLAB_CLI_COMMAND_SCL_NATIVE_WIRE_START = 14,
    UNITLAB_CLI_COMMAND_SCL_DISCOVER_PROBE = 15,
    UNITLAB_CLI_COMMAND_SCL_METADATA_PROBE = 16,
    UNITLAB_CLI_COMMAND_SCL_GI_PROBE = 17,
    UNITLAB_CLI_COMMAND_MMS_CLIENT_START = 18
} UnitLabCliCommandKind;

typedef struct UnitLabCliOptions {
    const char* fixture_path;
    const char* scl_path;
    const char* ied_name;
    const char* bind_address;
    int port;
    int dry_run;
    int smoke_start;
    int native_smoke_start;
    int native_wire_start;
    int native_wire_client_start;
    int mms_client_start;
    int discover_probe;
    int metadata_probe;
    int gi_probe;
    int native_test_report_tick_ms;
    const char* report_key;
    const char* native_client_read_domain;
    const char* native_client_read_item;
    uint32_t native_client_read_invoke_id;
} UnitLabCliOptions;

typedef struct UnitLabCliContext {
    UnitLabCliOptions options;
    UnitLabCliSourceKind source_kind;
    UnitLabCliCommandKind command_kind;
    UnitLabIedServerConfig server_config;
    UnitLabNativeWireClientOptions wire_client_options;
} UnitLabCliContext;

int unitlab_cli_parse_int(const char* value, int* out);
int unitlab_cli_parse_uint32(const char* value, uint32_t* out);
UnitLabCliCommandKind unitlab_cli_select_fixture_command(const UnitLabCliOptions* options);
UnitLabCliCommandKind unitlab_cli_select_scl_command(const UnitLabCliOptions* options);
int unitlab_cli_parse_options(int argc, char** argv, UnitLabCliContext* context);
void unitlab_cli_print_usage(const char* program_name);

#endif /* UNITLAB_IEC61850_IED_APP_CLI_UNITLAB_CLI_OPTIONS_H */
