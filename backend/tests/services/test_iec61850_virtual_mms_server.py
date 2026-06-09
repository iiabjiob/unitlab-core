from __future__ import annotations

from pathlib import Path

import pytest

from app.services.iec61850.report_runtime import Iec61850ReportRuntimeError
from app.services.iec61850.virtual_mms_server import _validate_native_binary_path


def test_virtual_mms_server_reports_missing_native_binary() -> None:
    missing_binary = Path("/tmp/unitlab-missing-iec61850-ied-sim")

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        _validate_native_binary_path(missing_binary)

    assert error.value.code == "VIRTUAL_MMS_BINARY_NOT_FOUND"
    assert str(missing_binary) in str(error.value)
