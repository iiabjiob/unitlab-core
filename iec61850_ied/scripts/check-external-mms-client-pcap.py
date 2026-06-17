#!/usr/bin/env python3
"""Validate SCD-backed external MMS client pcaps for behavior-level gates.

This checker is intentionally protocol-behavior oriented. It verifies that a
saved Wireshark/tshark capture contains the target IED/model namespace, RCB
metadata access, RptEna/GI writes, and report traffic without confirmed MMS
errors. It does not assert byte-identical parity with IEDScout.
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
    return [part for part in value.split(',') if part]


def load_mms_rows(pcap: Path, port: int) -> list[MmsRow]:
    cmd = [
        'tshark',
        '-r', str(pcap),
        '-d', f'tcp.port=={port},tpkt',
        '-Y', 'mms',
        '-T', 'fields',
        '-E', 'separator=	',
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
        fields = line.split('	')
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


def find_request(rows: list[MmsRow], service: str, token: str, pcap: Path, port: int) -> str | None:
    for row in rows:
        if service not in row.requests or not row.invoke_ids:
            continue
        if token in row.info or token in frame_verbose_text(pcap, port, row.frame):
            return row.invoke_ids[0]
    return None


def has_response(rows: list[MmsRow], invoke_id: str, service: str) -> bool:
    return any(service in row.responses and invoke_id in row.invoke_ids for row in rows)


def main() -> int:
    parser = argparse.ArgumentParser(description='Validate SCD-backed external MMS client pcap gates.')
    parser.add_argument('pcap', type=Path)
    parser.add_argument('--port', type=int, default=12447)
    parser.add_argument('--ied', default='KINTE13LVC01')
    parser.add_argument('--domain', default='KINTE13LVC01CTRL')
    parser.add_argument('--rcb', default='LLN0$BR$brcbA')
    parser.add_argument('--dataset', default='LLN0$RCB1')
    parser.add_argument('--min-reports', type=int, default=1)
    parser.add_argument('--allow-tcp-reset', action='store_true')
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

    resets = tcp_reset_count(args.pcap, args.port)
    if resets and not args.allow_tcp_reset:
        failures.append(f'TCP reset observed: {resets}')

    if any(row.errors for row in rows):
        failures.append('confirmed-ErrorPDU observed')

    required_tokens = [args.ied, args.domain, args.dataset, args.rcb]
    for token in required_tokens:
        if not capture_contains(rows, args.pcap, args.port, token):
            failures.append(f'missing token in MMS decode: {token}')

    invoke = find_request(rows, '4', args.rcb, args.pcap, args.port)
    if invoke is None:
        failures.append(f'missing Read request for {args.rcb}')
    elif not has_response(rows, invoke, '4'):
        failures.append(f'missing Read response for invoke {invoke} ({args.rcb})')

    for field in ['RptEna', 'GI']:
        token = f'{args.rcb}${field}'
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
    print(f'mms_requests={request_count} mms_responses={response_count} information_reports={report_count} tcp_resets={resets}')
    if failures:
        for failure in failures:
            print(f'FAIL: {failure}')
        return 1
    print('PASS: external MMS client pcap gates satisfied')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
