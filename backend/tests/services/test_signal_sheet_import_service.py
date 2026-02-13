from __future__ import annotations

from io import BytesIO

from openpyxl import Workbook

from app.schemas.signal_snapshot_schema import SignalImportMetaSchema
from app.services.signal_sheet_import_service import SignalSheetImportService


def _build_workbook(rows: list[list[object]], title: str = "Signals") -> bytes:
    wb = Workbook()
    ws = wb.active
    ws.title = title
    for row in rows:
        ws.append(row)
    buff = BytesIO()
    wb.save(buff)
    return buff.getvalue()


def test_parse_workbook_projects_signals_from_internal_type() -> None:
    raw = _build_workbook(
        [
            ["Signal Name", "VendorType", "internal_type"],
            ["Pump Start", "DIGITAL OUTPUT", "do"],
            ["Pump Feedback", "DIGITAL INPUT", "di"],
            ["Not Mapped", "UNKNOWN", None],
        ]
    )
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        hmi_representation="Signal Name",
        type_column="VendorType",
        internal_type_column="internal_type",
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="demo.xlsx", metadata=meta)

    assert payload.data["version"] == 2
    assert payload.data["sheet_count"] == 1
    assert payload.rows_count == 3
    assert len(payload.signals) == 2

    first = payload.signals[0]
    second = payload.signals[1]

    assert first.key == "pump_start"
    assert first.io_direction == "DO"
    assert first.name == "Pump Start"

    assert second.key == "pump_feedback"
    assert second.io_direction == "DI"


def test_parse_workbook_projects_signals_using_type_mapping_fallback() -> None:
    raw = _build_workbook(
        [
            ["Name", "Type"],
            ["Breaker Close", "Digital Output"],
            ["Breaker Open", "Digital Input"],
        ]
    )
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        hmi_representation="Name",
        type_column="Type",
        type_mapping={
            "Digital Output": "do",
            "Digital Input": "di",
        },
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="vendor.xlsx", metadata=meta)

    assert len(payload.signals) == 2
    assert {signal.io_direction for signal in payload.signals} == {"DI", "DO"}
