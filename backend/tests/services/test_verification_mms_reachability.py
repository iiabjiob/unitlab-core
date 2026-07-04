from __future__ import annotations

import asyncio
import errno

import pytest

from app.schemas.verification_schema import (
    VerificationMmsReachabilityRequestSchema,
    VerificationMmsReachabilityTargetSchema,
)
from app.services import verification_mms_reachability
from app.services.verification_mms_reachability import check_mms_tcp_reachability


class _FakeWriter:
    def __init__(self) -> None:
        self.closed = False

    def close(self) -> None:
        self.closed = True

    async def wait_closed(self) -> None:
        return None


@pytest.mark.anyio
async def test_check_mms_tcp_reachability_reports_reachable(monkeypatch) -> None:
    writer = _FakeWriter()

    async def fake_open_connection(host: str, port: int):
        assert host == "10.10.10.20"
        assert port == 12447
        return object(), writer

    monkeypatch.setattr(verification_mms_reachability.asyncio, "open_connection", fake_open_connection)

    response = await check_mms_tcp_reachability(
        VerificationMmsReachabilityRequestSchema(
            targets=[VerificationMmsReachabilityTargetSchema(host="10.10.10.20", port=12447)],
            timeout_ms=500,
        )
    )

    assert len(response.results) == 1
    assert response.results[0].host == "10.10.10.20"
    assert response.results[0].port == 12447
    assert response.results[0].reachable is True
    assert response.results[0].error is None
    assert response.results[0].check_kind == "tcp_connect"
    assert response.results[0].failure_code is None
    assert writer.closed is True


@pytest.mark.anyio
async def test_check_mms_tcp_reachability_classifies_timeout(monkeypatch) -> None:
    async def fake_open_connection(_host: str, _port: int):
        raise TimeoutError("connect timed out")

    monkeypatch.setattr(verification_mms_reachability.asyncio, "open_connection", fake_open_connection)

    response = await check_mms_tcp_reachability(
        VerificationMmsReachabilityRequestSchema(
            targets=[VerificationMmsReachabilityTargetSchema(host="10.10.10.21", port=102)],
            timeout_ms=500,
        )
    )

    assert response.results[0].reachable is False
    assert response.results[0].failure_code == "unreachable"
    assert "timeout" in str(response.results[0].error).lower()


@pytest.mark.anyio
async def test_check_mms_tcp_reachability_classifies_connection_refused(monkeypatch) -> None:
    async def fake_open_connection(_host: str, _port: int):
        raise ConnectionRefusedError("connection refused")

    monkeypatch.setattr(verification_mms_reachability.asyncio, "open_connection", fake_open_connection)

    response = await check_mms_tcp_reachability(
        VerificationMmsReachabilityRequestSchema(
            targets=[VerificationMmsReachabilityTargetSchema(host="10.10.10.22", port=102)],
            timeout_ms=500,
        )
    )

    assert response.results[0].reachable is False
    assert response.results[0].failure_code == "mms_unavailable"
    assert "refused" in str(response.results[0].error).lower()


@pytest.mark.anyio
async def test_check_mms_tcp_reachability_classifies_network_unreachable(monkeypatch) -> None:
    async def fake_open_connection(_host: str, _port: int):
        raise OSError(errno.ENETUNREACH, "network unreachable")

    monkeypatch.setattr(verification_mms_reachability.asyncio, "open_connection", fake_open_connection)

    response = await check_mms_tcp_reachability(
        VerificationMmsReachabilityRequestSchema(
            targets=[VerificationMmsReachabilityTargetSchema(host="10.10.10.23", port=102)],
            timeout_ms=500,
        )
    )

    assert response.results[0].reachable is False
    assert response.results[0].failure_code == "network_unreachable"


@pytest.mark.anyio
async def test_check_mms_tcp_reachability_limits_concurrency(monkeypatch) -> None:
    active = 0
    max_active = 0

    async def fake_open_connection(_host: str, _port: int):
        nonlocal active, max_active
        active += 1
        max_active = max(max_active, active)
        await asyncio.sleep(0.01)
        active -= 1
        return object(), _FakeWriter()

    monkeypatch.setattr(verification_mms_reachability.asyncio, "open_connection", fake_open_connection)

    response = await check_mms_tcp_reachability(
        VerificationMmsReachabilityRequestSchema(
            targets=[
                VerificationMmsReachabilityTargetSchema(host=f"10.10.10.{index}", port=102)
                for index in range(10, 15)
            ],
            timeout_ms=500,
            concurrency=2,
        )
    )

    assert len(response.results) == 5
    assert max_active <= 2
