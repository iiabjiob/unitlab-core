#include "app/cli/unitlab_cli.h"

/*
 * Thin entrypoint only. CLI parsing, dispatch, and command execution live in
 * the app/cli modules so modes stay isolated and easy to extend.
 */
int main(int argc, char** argv)
{
    return unitlab_cli_run(argc, argv);
}
