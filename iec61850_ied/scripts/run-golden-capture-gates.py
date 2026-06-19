#!/usr/bin/env python3
"""Run manifest-driven IEC 61850 golden capture gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open('r', encoding='utf-8') as handle:
        data = json.load(handle)
    if data.get('version') != 1:
        raise ValueError(f'unsupported manifest version: {data.get("version")}')
    if not isinstance(data.get('cases'), list):
        raise ValueError('manifest cases must be a list')
    return data


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def case_command(root: Path, case: dict[str, Any]) -> list[str]:
    checker = case.get('checker')
    pcap = case.get('pcap')
    if not isinstance(checker, str) or not isinstance(pcap, str):
        raise ValueError(f'case {case.get("name", "<unnamed>")} requires checker and pcap')
    args = case.get('args', [])
    if not isinstance(args, list) or not all(isinstance(arg, str) for arg in args):
        raise ValueError(f'case {case.get("name", "<unnamed>")} args must be a string list')
    return [sys.executable, str(root / checker), str(root / pcap), *args]


def selected_cases(cases: list[dict[str, Any]], names: list[str]) -> list[dict[str, Any]]:
    if not names:
        return cases
    selected = []
    known = {case.get('name') for case in cases}
    for name in names:
        if name not in known:
            raise ValueError(f'unknown golden capture case: {name}')
        selected.extend(case for case in cases if case.get('name') == name)
    return selected


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser(description='Run IEC 61850 golden capture gates from a manifest.')
    parser.add_argument('--manifest', type=Path, default=root / 'docs' / 'golden-captures.json')
    parser.add_argument('--case', action='append', default=[], help='Run one named case; may be repeated.')
    parser.add_argument('--require-tools', action='store_true', help='Fail instead of skip when tshark is unavailable.')
    args = parser.parse_args()

    if shutil.which('tshark') is None:
        message = 'FAIL: tshark is required for golden capture gates' if args.require_tools else 'SKIP: tshark is unavailable'
        print(message)
        return 2 if args.require_tools else 0

    try:
        manifest = load_manifest(args.manifest)
        cases = selected_cases(manifest['cases'], args.case)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f'FAIL: {exc}')
        return 2

    failures = 0
    print(f'golden_manifest={args.manifest}')
    for case in cases:
        name = case.get('name', '<unnamed>')
        pcap_value = case.get('pcap')
        if not isinstance(pcap_value, str):
            print(f'FAIL: {name}: missing pcap path')
            failures += 1
            continue

        pcap = root / pcap_value
        print(f'case={name} pcap={pcap_value}')
        if not pcap.exists():
            print(f'FAIL: {name}: pcap not found: {pcap}')
            failures += 1
            continue

        expected_sha256 = case.get('sha256')
        if expected_sha256 is not None:
            actual_sha256 = sha256_file(pcap)
            if actual_sha256 != expected_sha256:
                print(f'FAIL: {name}: sha256 mismatch expected={expected_sha256} actual={actual_sha256}')
                failures += 1
                continue

        try:
            command = case_command(root, case)
        except ValueError as exc:
            print(f'FAIL: {exc}')
            failures += 1
            continue

        result = subprocess.run(command, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.stdout:
            print(result.stdout, end='' if result.stdout.endswith('\n') else '\n')
        if result.stderr:
            print(result.stderr, end='' if result.stderr.endswith('\n') else '\n', file=sys.stderr)
        if result.returncode != 0:
            print(f'FAIL: {name}: checker exited {result.returncode}')
            failures += 1

    if failures:
        print(f'FAIL: golden capture gates failed cases={failures}/{len(cases)}')
        return 1
    print(f'PASS: golden capture gates satisfied cases={len(cases)}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
