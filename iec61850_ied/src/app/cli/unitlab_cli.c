#include "app/cli/unitlab_cli_internal.h"

int unitlab_cli_run(int argc, char** argv)
{
    UnitLabCliContext context;
    int parsed = unitlab_cli_parse_options(argc, argv, &context);
    if (parsed > 0) {
        return 0;
    }
    if (parsed < 0) {
        return 64;
    }

    return unitlab_cli_dispatch(&context);
}
