#include "app/cli/commands/unitlab_cmd_mms_client.h"

/*
 * Persistent native MMS client CLI command. It owns the stdin-driven client
 * runtime and emits the current compatibility diagnostics.
 */

#include <stdio.h>

#include "app/cli/unitlab_cli_internal.h"

int unitlab_cli_run_mms_client_command(const UnitLabCliContext* context)
{
    const UnitLabCliOptions* options;
    UnitLabIedModelLoadResult load_result;
    UnitLabIedServerConfig client_config;

    if (context == NULL) {
        fprintf(stderr, "INVALID_ARGUMENT: missing CLI context.\n");
        return 64;
    }

    options = &context->options;
    client_config = context->server_config;
    client_config.control_port = 0;

    unitlab_cli_install_stop_handlers();
    if (!unitlab_run_native_wire_client_with_options(&client_config, &context->wire_client_options, &load_result, unitlab_cli_signal_stop_requested, NULL)) {
        fprintf(stderr, "%s: %s\n", load_result.code, load_result.message);
        fprintf(stderr, "mms-backend=%s\n", unitlab_cli_mms_backend_status());
        return 69;
    }
    printf("unitlab-iec61850-ied-sim: persistent native MMS client stopped\n");
    printf("ied=%s\n", options->ied_name != NULL ? options->ied_name : "");
    printf("endpoint=%s:%d\n", options->bind_address, options->port);
    printf("mms-backend=%s\n", unitlab_cli_mms_backend_status());
    return 0;
}
