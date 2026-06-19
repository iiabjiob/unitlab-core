#!/usr/bin/env python3
"""Validate SCD-backed IEC 61850 discovery pcaps for behavior-level gates."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class MmsRow:
    frame: str
    info: str
    requests: list[str]
    responses: list[str]
    errors: str


def run_tshark(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def split_multi(value: str) -> list[str]:
    return [part for part in value.split(',') if part]


def load_rows(pcap: Path, port: int) -> list[MmsRow]:
    cmd = [
        'tshark',
        '-r', str(pcap),
        '-d', f'tcp.port=={port},tpkt',
        '-Y', 'mms',
        '-T', 'fields',
        '-E', 'separator=\t',
        '-e', 'frame.number',
        '-e', '_ws.col.Info',
        '-e', 'mms.confirmedServiceRequest',
        '-e', 'mms.confirmedServiceResponse',
        '-e', 'mms.confirmed_ErrorPDU_element',
    ]
    result = run_tshark(cmd)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or 'tshark failed')
    rows: list[MmsRow] = []
    for line in result.stdout.splitlines():
        fields = line.split('\t')
        fields.extend([''] * (5 - len(fields)))
        rows.append(MmsRow(
            frame=fields[0],
            info=fields[1],
            requests=split_multi(fields[2]),
            responses=split_multi(fields[3]),
            errors=fields[4],
        ))
    return rows


def tcp_reset_count(pcap: Path, port: int) -> int:
    display_filter = f'tcp.flags.reset == 1 && tcp.port == {port}'
    result = run_tshark(['tshark', '-r', str(pcap), '-Y', display_filter, '-T', 'fields', '-e', 'frame.number'])
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or 'tshark TCP reset scan failed')
    return len([line for line in result.stdout.splitlines() if line.strip()])


def frame_verbose_text(pcap: Path, port: int, frame: str) -> str:
    result = run_tshark(['tshark', '-r', str(pcap), '-d', f'tcp.port=={port},tpkt', '-Y', f'frame.number=={frame}', '-V'])
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or f'tshark verbose decode failed for frame {frame}')
    return result.stdout


def capture_contains(rows: list[MmsRow], pcap: Path, port: int, token: str) -> bool:
    for row in rows:
        if token in row.info:
            return True
        if token in frame_verbose_text(pcap, port, row.frame):
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description='Validate SCD-backed IEC 61850 discovery pcap gates.')
    parser.add_argument('pcap', type=Path)
    parser.add_argument('--port', type=int, default=12447)
    parser.add_argument('--ied', default='KINTE13LVC01')
    parser.add_argument('--domain', default='KINTE13LVC01CTRL')
    parser.add_argument('--dataset', action='append', default=['LLN0$RCB1', 'LLN0$RCB2'])
    parser.add_argument('--rcb', action='append', default=['LLN0$BR$brcbA', 'LLN0$BR$brcbB'])
    parser.add_argument('--min-requests', type=int, default=20)
    args = parser.parse_args()

    if shutil.which('tshark') is None:
        print('FAIL: tshark is required', file=sys.stderr)
        return 2
    if not args.pcap.exists():
        print(f'FAIL: pcap not found: {args.pcap}', file=sys.stderr)
        return 2

    rows = load_rows(args.pcap, args.port)
    failures: list[str] = []
    if not rows:
        failures.append('no MMS frames decoded; check --port/decode-as')

    resets = tcp_reset_count(args.pcap, args.port)
    if resets:
        failures.append(f'TCP reset observed: {resets}')
    if any(row.errors for row in rows):
        failures.append('confirmed-ErrorPDU observed')

    request_count = sum(len(row.requests) for row in rows)
    response_count = sum(len(row.responses) for row in rows)
    if request_count < args.min_requests:
        failures.append(f'expected at least {args.min_requests} MMS requests, saw {request_count}')

    required_tokens = [args.ied, args.domain, *args.dataset, *args.rcb]
    for token in required_tokens:
        if not capture_contains(rows, args.pcap, args.port, token):
            failures.append(f'missing token in MMS decode: {token}')

    print(f'pcap={args.pcap}')
    print(f'mms_requests={request_count} mms_responses={response_count} tcp_resets={resets}')
    if failures:
        for failure in failures:
            print(f'FAIL: {failure}')
        return 1
    print('PASS: SCD discovery pcap gates satisfied')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
