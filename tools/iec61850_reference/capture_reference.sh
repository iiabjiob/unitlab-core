#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pcap_path="${CAPTURE_PCAP:-${script_dir}/captures/libiec61850-reference.pcap}"
capture_port="${CAPTURE_PORT:-12449}"
capture_interface="${CAPTURE_INTERFACE:-any}"

mkdir -p "$(dirname "${pcap_path}")"

if [[ $# -gt 0 ]]; then
    exec sudo tcpdump -i "${capture_interface}" -s 0 -w "${pcap_path}" "tcp port ${capture_port}" "$@"
fi

exec sudo tcpdump -i "${capture_interface}" -s 0 -w "${pcap_path}" "tcp port ${capture_port}"
