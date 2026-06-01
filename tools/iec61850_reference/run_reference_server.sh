#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

server_bin="${LIBIEC61850_SERVER_BIN:-}"
if [[ -z "${server_bin}" ]]; then
    lib_root="${LIBIEC61850_ROOT:-${repo_root}/external/libiec61850}"
    for candidate in \
        "${lib_root}/examples/server_example_basic_io/server_example_basic_io" \
        "${lib_root}/build/examples/server_example_basic_io/server_example_basic_io" \
        "${lib_root}/examples/server_example_basic_io/server_example_basic_io.exe"; do
        if [[ -x "${candidate}" ]]; then
            server_bin="${candidate}"
            break
        fi
    done
fi

if [[ -z "${server_bin}" || ! -x "${server_bin}" ]]; then
    cat <<'EOF' >&2
Set LIBIEC61850_SERVER_BIN to the external reference server executable.
Example:
  export LIBIEC61850_SERVER_BIN=/workspace/external/libiec61850/examples/server_example_basic_io/server_example_basic_io
EOF
    exit 1
fi

if [[ $# -gt 0 ]]; then
    exec "${server_bin}" "$@"
fi

if [[ -n "${LIBIEC61850_SERVER_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    server_args=(${LIBIEC61850_SERVER_ARGS})
    exec "${server_bin}" "${server_args[@]}"
fi

exec "${server_bin}"

