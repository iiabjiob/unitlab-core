#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

client_bin="${LIBIEC61850_CLIENT_BIN:-}"
if [[ -z "${client_bin}" ]]; then
    lib_root="${LIBIEC61850_ROOT:-${repo_root}/external/libiec61850}"
    for candidate in \
        "${lib_root}/examples/client_example_basic_io/client_example_basic_io" \
        "${lib_root}/build/examples/client_example_basic_io/client_example_basic_io" \
        "${lib_root}/examples/client_example_basic_io/client_example_basic_io.exe"; do
        if [[ -x "${candidate}" ]]; then
            client_bin="${candidate}"
            break
        fi
    done
fi

if [[ -z "${client_bin}" || ! -x "${client_bin}" ]]; then
    cat <<'EOF' >&2
Set LIBIEC61850_CLIENT_BIN to the external reference client executable.
Example:
  export LIBIEC61850_CLIENT_BIN=/workspace/external/libiec61850/examples/client_example_basic_io/client_example_basic_io
EOF
    exit 1
fi

if [[ $# -gt 0 ]]; then
    exec "${client_bin}" "$@"
fi

if [[ -n "${LIBIEC61850_CLIENT_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    client_args=(${LIBIEC61850_CLIENT_ARGS})
else
    client_args=(localhost 12449)
fi

exec "${client_bin}" "${client_args[@]}"

