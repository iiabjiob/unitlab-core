#!/usr/bin/env python3
"""Validate native-wire MMS pcap checkpoints used for v1 report interop.

This is intentionally a coarse gate over actual pcaps captured with IEDScout:
it verifies that the session reaches discovery, RCB enable, GI, and report traffic
without confirmed errors or TCP reset. It does not replace byte-level debugging.
"""

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
    invoke_ids: list[str]
    requests: list[str]
    responses: list[str]
    unconfirmed: list[str]
    errors: str


def run_tshark(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def split_multi(value: str) -> list[str]:
    return [part for part in value.split(',') if part != '']


def load_mms_rows(pcap: Path, port: int) -> list[MmsRow]:
    cmd = [
        'tshark',
        '-r', str(pcap),
        '-d', f'tcp.port=={port},tpkt',
        '-Y', 'mms',
        '-T', 'fields',
        '-E', 'separator=\t',
        '-e', 'frame.number',
        '-e', '_ws.col.Info',
        '-e', 'mms.invokeID',
        '-e', 'mms.confirmedServiceRequest',
        '-e', 'mms.confirmedServiceResponse',
        '-e', 'mms.unconfirmedService',
        '-e', 'mms.confirmed_ErrorPDU_element',
    ]
    result = run_tshark(cmd)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or 'tshark failed')
    rows: list[MmsRow] = []
    for line in result.stdout.splitlines():
        fields = line.split('\t')
        fields.extend([''] * (7 - len(fields)))
        rows.append(MmsRow(
            frame=fields[0],
            info=fields[1],
            invoke_ids=split_multi(fields[2]),
            requests=split_multi(fields[3]),
            responses=split_multi(fields[4]),
            unconfirmed=split_multi(fields[5]),
            errors=fields[6],
        ))
    return rows


def tcp_reset_count(pcap: Path) -> int:
    result = run_tshark([
        'tshark', '-r', str(pcap), '-Y', 'tcp.flags.reset == 1', '-T', 'fields', '-e', 'frame.number'
    ])
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or 'tshark TCP reset scan failed')
    return len([line for line in result.stdout.splitlines() if line.strip()])


def frame_verbose_text(pcap: Path, port: int, frame: str) -> str:
    result = run_tshark([
        'tshark', '-r', str(pcap), '-d', f'tcp.port=={port},tpkt', '-Y', f'frame.number=={frame}', '-V'
    ])
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or f'tshark verbose decode failed for frame {frame}')
    return result.stdout


def find_request(rows: list[MmsRow], service: str, token: str, pcap: Path, port: int) -> str | None:
    for row in rows:
        if service not in row.requests or not row.invoke_ids:
            continue
        if token in row.info or token in frame_verbose_text(pcap, port, row.frame):
            return row.invoke_ids[0]
    return None


def has_response(rows: list[MmsRow], invoke_id: str, service: str) -> bool:
    for row in rows:
        if service in row.responses and invoke_id in row.invoke_ids:
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description='Validate native-wire MMS pcap v1 gates.')
    parser.add_argument('pcap', type=Path)
    parser.add_argument('--port', type=int, default=12447)
    parser.add_argument('--min-reports', type=int, default=1)
    args = parser.parse_args()

    if shutil.which('tshark') is None:
        print('FAIL: tshark is required', file=sys.stderr)
        return 2
    if not args.pcap.exists():
        print(f'FAIL: pcap not found: {args.pcap}', file=sys.stderr)
        return 2

    rows = load_mms_rows(args.pcap, args.port)
    failures: list[str] = []
    if not rows:
        failures.append('no MMS frames decoded; check --port/decode-as')

    if tcp_reset_count(args.pcap) != 0:
        failures.append('TCP reset observed')

    if any(row.errors for row in rows):
        failures.append('confirmed-ErrorPDU observed')

    required_reads = [
        'LLN0$BR$brcbEvents',
        'XCBR1$ST',
        'PGGIO1$ST',
        'GGIO1$MX',
    ]
    for token in required_reads:
        invoke = find_request(rows, '4', token, args.pcap, args.port)
        if invoke is None:
            failures.append(f'missing Read request for {token}')
        elif not has_response(rows, invoke, '4'):
            failures.append(f'missing Read response for invoke {invoke} ({token})')

    required_writes = [
        'LLN0$BR$brcbEvents$ResvTms',
        'LLN0$BR$brcbEvents$RptEna',
        'LLN0$BR$brcbEvents$GI',
    ]
    for token in required_writes:
        invoke = find_request(rows, '5', token, args.pcap, args.port)
        if invoke is None:
            failures.append(f'missing Write request for {token}')
        elif not has_response(rows, invoke, '5'):
            failures.append(f'missing Write response for invoke {invoke} ({token})')

    report_count = sum(1 for row in rows if '0' in row.unconfirmed)
    if report_count < args.min_reports:
        failures.append(f'expected at least {args.min_reports} InformationReport frames, saw {report_count}')

    request_count = sum(len(row.requests) for row in rows)
    response_count = sum(len(row.responses) for row in rows)
    print(f'pcap={args.pcap}')
    print(f'mms_requests={request_count} mms_responses={response_count} information_reports={report_count}')
    if failures:
        for failure in failures:
            print(f'FAIL: {failure}')
        return 1
    print('PASS: native-wire v1 pcap gates satisfied')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
