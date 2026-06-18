#!/usr/bin/env python3
"""Validate IEC 61850 association pcaps for behavior-level gates."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class AssociationRow:
    frame: str
    info: str
    cotp_type: str
    cotp_eot: str
    aarq: str
    aare: str
    abrt: str
    initiate_request: str
    initiate_response: str
    initiate_error: str
    reject: str
    confirmed_error: str


def run_tshark(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def any_field(value: str) -> bool:
    return any(part for part in value.split(',') if part)


def field_contains(value: str, token: str) -> bool:
    return any(part == token for part in value.split(',') if part)


def false_field(value: str) -> bool:
    return any(part.lower() in ('0', 'false') for part in value.split(',') if part)


def load_rows(pcap: Path, port: int) -> list[AssociationRow]:
    cmd = [
        'tshark',
        '-r', str(pcap),
        '-d', f'tcp.port=={port},tpkt',
        '-Y', 'cotp || acse || mms',
        '-T', 'fields',
        '-E', 'separator=\t',
        '-e', 'frame.number',
        '-e', '_ws.col.Info',
        '-e', 'cotp.type',
        '-e', 'cotp.eot',
        '-e', 'acse.aarq_element',
        '-e', 'acse.aare_element',
        '-e', 'acse.abrt_element',
        '-e', 'mms.initiate_RequestPDU_element',
        '-e', 'mms.initiate_ResponsePDU_element',
        '-e', 'mms.initiate_ErrorPDU_element',
        '-e', 'mms.rejectPDU_element',
        '-e', 'mms.confirmed_ErrorPDU_element',
    ]
    result = run_tshark(cmd)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or 'tshark failed')

    rows: list[AssociationRow] = []
    for line in result.stdout.splitlines():
        fields = line.split('\t')
        fields.extend([''] * (12 - len(fields)))
        rows.append(AssociationRow(*fields[:12]))
    return rows


def tcp_reset_count(pcap: Path, port: int) -> int:
    display_filter = f'tcp.flags.reset == 1 && tcp.port == {port}'
    result = run_tshark(['tshark', '-r', str(pcap), '-Y', display_filter, '-T', 'fields', '-e', 'frame.number'])
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.strip() or 'tshark TCP reset scan failed')
    return len([line for line in result.stdout.splitlines() if line.strip()])


def main() -> int:
    parser = argparse.ArgumentParser(description='Validate IEC 61850 association pcap gates.')
    parser.add_argument('pcap', type=Path)
    parser.add_argument('--port', type=int, default=12448)
    parser.add_argument('--allow-tcp-reset', action='store_true')
    args = parser.parse_args()

    if shutil.which('tshark') is None:
        print('FAIL: tshark is required', file=sys.stderr)
        return 2
    if not args.pcap.exists():
        print(f'FAIL: pcap not found: {args.pcap}', file=sys.stderr)
        return 2

    rows = load_rows(args.pcap, args.port)
    resets = tcp_reset_count(args.pcap, args.port)

    cotp_cr = sum(1 for row in rows if field_contains(row.cotp_type, '0x0e') or 'CR TPDU' in row.info)
    cotp_cc = sum(1 for row in rows if field_contains(row.cotp_type, '0x0d') or 'CC TPDU' in row.info)
    segmented_dt = sum(
        1
        for row in rows
        if field_contains(row.cotp_type, '0x0f') and false_field(row.cotp_eot)
    )
    aarq = sum(1 for row in rows if any_field(row.aarq) or 'aarq' in row.info)
    aare = sum(1 for row in rows if any_field(row.aare) or 'aare' in row.info)
    initiate_requests = sum(
        1
        for row in rows
        if any_field(row.initiate_request) or 'initiate-RequestPDU' in row.info
    )
    initiate_responses = sum(
        1
        for row in rows
        if any_field(row.initiate_response) or 'initiate-ResponsePDU' in row.info
    )
    rejects = sum(
        1
        for row in rows
        if any_field(row.abrt)
        or any_field(row.initiate_error)
        or any_field(row.reject)
        or any_field(row.confirmed_error)
    )

    failures: list[str] = []
    if not rows:
        failures.append('no association frames decoded; check --port/decode-as')
    if cotp_cr < 1:
        failures.append('missing COTP CR frame')
    if cotp_cc < 1:
        failures.append('missing COTP CC frame')
    if aarq < 1:
        failures.append('missing ACSE AARQ frame')
    if aare < 1:
        failures.append('missing ACSE AARE frame')
    if initiate_requests < 1:
        failures.append('missing MMS Initiate request')
    if initiate_responses < 1:
        failures.append('missing MMS Initiate response')
    if segmented_dt:
        failures.append(f'association DT segmentation observed: {segmented_dt}')
    if rejects:
        failures.append(f'association reject/abort/error frames observed: {rejects}')
    if resets and not args.allow_tcp_reset:
        failures.append(f'TCP reset observed: {resets}')

    print(f'pcap={args.pcap}')
    print(
        'cotp_cr={cotp_cr} cotp_cc={cotp_cc} aarq={aarq} aare={aare} '
        'initiate_requests={initiate_requests} initiate_responses={initiate_responses} '
        'segmented_dt={segmented_dt} rejects={rejects} tcp_resets={resets}'.format(
            cotp_cr=cotp_cr,
            cotp_cc=cotp_cc,
            aarq=aarq,
            aare=aare,
            initiate_requests=initiate_requests,
            initiate_responses=initiate_responses,
            segmented_dt=segmented_dt,
            rejects=rejects,
            resets=resets,
        )
    )
    if failures:
        for failure in failures:
            print(f'FAIL: {failure}')
        return 1
    print('PASS: association pcap gates satisfied')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
