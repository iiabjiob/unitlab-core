from __future__ import annotations

from app.services.iec61850 import Iec61850MmsClientEvent, Iec61850MmsClientRuntime, Iec61850SclImportService
import app.services.iec61850 as iec61850


def test_iec61850_package_exports_client_runtime_boundary() -> None:
    assert Iec61850MmsClientEvent is iec61850.Iec61850MmsClientEvent
    assert Iec61850MmsClientRuntime is iec61850.Iec61850MmsClientRuntime
    assert "Iec61850MmsClientEvent" in iec61850.__all__
    assert "Iec61850MmsClientRuntime" in iec61850.__all__
    assert Iec61850SclImportService is iec61850.Iec61850SclImportService
    assert "Iec61850SclImportService" in iec61850.__all__
