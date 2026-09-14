#!/usr/bin/env bash
set -euo pipefail

test_python="${1:-python}"

if ! "${test_python}" -c 'import pyiec61850' >/dev/null 2>&1; then
  echo "SKIP: optional pyiec61850 wrapper is not installed; build external/libiec61850 first."
  exit 0
fi

PYTHONPATH="external/libiec61850/pyiec61850${PYTHONPATH:+:${PYTHONPATH}}" \
  "${test_python}" -m pytest -q external/libiec61850/pyiec61850/test_pyiec61850.py
