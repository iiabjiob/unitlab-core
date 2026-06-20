#include "app/cli/unitlab_cli_dispatch.h"

#include <stdio.h>

#include "app/cli/commands/unitlab_cmd_fixture.h"
#include "app/cli/commands/unitlab_cmd_mms_client.h"
#include "app/cli/commands/unitlab_cmd_scl.h"

typedef struct UnitLabCliCommandEntry {
    UnitLabCliCommandKind kind;
    int (*run)(const UnitLabCliContext* context);
} UnitLabCliCommandEntry;

/*
 * Add a new CLI mode by adding the command kind here and wiring it to the
 * appropriate runner. Keep command selection centralized in this table.
 */
static const UnitLabCliCommandEntry g_command_table[] = {
    {UNITLAB_CLI_COMMAND_FIXTURE_DRY_RUN, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_SERVER, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_SMOKE_START, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_SMOKE_START, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_WIRE_START, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_NATIVE_WIRE_CLIENT_START, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_DISCOVER_PROBE, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_METADATA_PROBE, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_FIXTURE_GI_PROBE, unitlab_cli_run_fixture_command},
    {UNITLAB_CLI_COMMAND_SCL_DRY_RUN, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_SERVER, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_SMOKE_START, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_NATIVE_SMOKE_START, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_NATIVE_WIRE_START, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_DISCOVER_PROBE, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_METADATA_PROBE, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_SCL_GI_PROBE, unitlab_cli_run_scl_command},
    {UNITLAB_CLI_COMMAND_MMS_CLIENT_START, unitlab_cli_run_mms_client_command},
};

int unitlab_cli_dispatch(const UnitLabCliContext* context)
{
    if (context == NULL || context->command_kind == UNITLAB_CLI_COMMAND_INVALID) {
        fprintf(stderr, "INVALID_ARGUMENT: no CLI command selected.\n");
        return 64;
    }

    for (size_t index = 0; index < sizeof(g_command_table) / sizeof(g_command_table[0]); index++) {
        if (g_command_table[index].kind == context->command_kind) {
            return g_command_table[index].run(context);
        }
    }

    fprintf(stderr, "INVALID_ARGUMENT: unsupported CLI command kind %d.\n", (int)context->command_kind);
    return 64;
}
