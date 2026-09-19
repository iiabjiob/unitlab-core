from __future__ import annotations

import asyncio
import errno
from datetime import datetime, timezone
from typing import Literal, cast

from app.schemas.verification_schema import (
    VerificationMmsReachabilityRequestSchema,
    VerificationMmsReachabilityResponseSchema,
    VerificationMmsReachabilityResultSchema,
    VerificationMmsReachabilityTargetSchema,
)


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _classify_tcp_error(exc: Exception) -> tuple[str, str]:
    if isinstance(exc, TimeoutError):
        return "unreachable", "TCP connect timeout"
    if isinstance(exc, ConnectionRefusedError):
        return "mms_unavailable", "TCP connection refused"
    if isinstance(exc, PermissionError):
        return "probe_failed", str(exc) or "TCP probe permission denied"
    if isinstance(exc, OSError):
        if exc.errno in {errno.ENETUNREACH, errno.EHOSTUNREACH, errno.ENETDOWN, errno.EHOSTDOWN}:
            return "network_unreachable", str(exc) or "Network unreachable"
        if exc.errno in {errno.EACCES, errno.EPERM}:
            return "probe_failed", str(exc) or "TCP probe permission denied"
        return "probe_failed", str(exc) or exc.__class__.__name__
    return "probe_failed", str(exc) or exc.__class__.__name__


async def check_mms_tcp_endpoint(
    target: VerificationMmsReachabilityTargetSchema,
    *,
    timeout_ms: int,
) -> VerificationMmsReachabilityResultSchema:
    try:
        _reader, writer = await asyncio.wait_for(
            asyncio.open_connection(target.host, target.port),
            timeout=max(0.1, timeout_ms / 1000),
        )
        writer.close()
        await writer.wait_closed()
        return VerificationMmsReachabilityResultSchema(
            host=target.host,
            port=target.port,
            reachable=True,
            checked_at=_utc_now_iso(),
            error=None,
            check_kind="tcp_connect",
            failure_code=None,
        )
    except Exception as exc:  # noqa: BLE001
        failure_code, message = _classify_tcp_error(exc)
        return VerificationMmsReachabilityResultSchema(
            host=target.host,
            port=target.port,
            reachable=False,
            checked_at=_utc_now_iso(),
            error=message,
            check_kind="tcp_connect",
            failure_code=cast(
                Literal["unreachable", "mms_unavailable", "network_unreachable", "probe_failed"] | None,
                failure_code,
            ),
        )


async def check_mms_tcp_reachability(
    payload: VerificationMmsReachabilityRequestSchema,
) -> VerificationMmsReachabilityResponseSchema:
    semaphore = asyncio.Semaphore(max(1, min(16, int(payload.concurrency))))

    async def run(target: VerificationMmsReachabilityTargetSchema) -> VerificationMmsReachabilityResultSchema:
        async with semaphore:
            return await check_mms_tcp_endpoint(target, timeout_ms=payload.timeout_ms)

    results = await asyncio.gather(*(run(target) for target in payload.targets))
    return VerificationMmsReachabilityResponseSchema(results=list(results))
