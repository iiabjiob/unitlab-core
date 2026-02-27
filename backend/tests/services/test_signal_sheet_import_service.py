from __future__ import annotations

from io import BytesIO

from openpyxl import Workbook

from app.schemas.signal_import_schema import SignalImportMetaSchema
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
        type_column="Type",
        type_mapping={
            "Digital Output": "do",
            "Digital Input": "di",
        },
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="vendor.xlsx", metadata=meta)

    assert len(payload.signals) == 2
    assert {signal.io_direction for signal in payload.signals} == {"DI", "DO"}


def test_parse_workbook_expands_dpc_row_into_two_do_signals() -> None:
    raw = _build_workbook(
        [
            ["Signal Name", "Type", "Terminal"],
            ["QF1 Position Command", "DPC", "X1:12/X1:13"],
        ]
    )
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        type_column="Type",
        terminal_column="Terminal",
        type_mapping={"DPC": "do"},
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="dpc.xlsx", metadata=meta)

    assert len(payload.signals) == 2
    assert [s.io_direction for s in payload.signals] == ["DO", "DO"]
    assert payload.signals[0].name.endswith("OPEN CMD")
    assert payload.signals[1].name.endswith("CLOSE CMD")

    packed0 = payload.signals[0].signal_metadata.get("packed_group")
    packed1 = payload.signals[1].signal_metadata.get("packed_group")
    assert isinstance(packed0, dict)
    assert isinstance(packed1, dict)
    assert packed0["kind"] == "dpc"
    assert packed1["kind"] == "dpc"
    assert packed0["source_group_id"] == packed1["source_group_id"]
    assert packed0["position"] == 0
    assert packed1["position"] == 1
    # terminal column is transparently split per expanded signal row
    assert payload.signals[0].signal_metadata["row"]["Terminal"] == "X1:12"
    assert payload.signals[1].signal_metadata["row"]["Terminal"] == "X1:13"


def test_parse_workbook_expands_dps_alias_without_explicit_mapping() -> None:
    raw = _build_workbook(
        [
            ["Name", "Signal Type"],
            ["QF1 Position Feedback", "Double Point Status"],
        ]
    )
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        type_column="Signal Type",
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="dps.xlsx", metadata=meta)

    assert len(payload.signals) == 2
    assert [s.io_direction for s in payload.signals] == ["DI", "DI"]
    assert payload.signals[0].name.endswith("OPEN FB")
    assert payload.signals[1].name.endswith("CLOSE FB")


def test_parse_workbook_keeps_terminal_unsplit_if_no_separator() -> None:
    raw = _build_workbook(
        [
            ["Name", "Type", "Terminal"],
            ["QF2 Cmd", "DPC", "XT2:11"],
        ]
    )
    meta = SignalImportMetaSchema(sheet_name="Signals", type_column="Type", terminal_column="Terminal")

    payload = SignalSheetImportService.parse_workbook(raw, filename="dpc_single_terminal.xlsx", metadata=meta)

    assert len(payload.signals) == 2
    assert payload.signals[0].signal_metadata["row"]["Terminal"] == "XT2:11"
    assert payload.signals[1].signal_metadata["row"]["Terminal"] == "XT2:11"


def test_parse_workbook_projects_only_selected_columns_for_metadata_row() -> None:
    raw = _build_workbook(
        [
            ["Name", "Type", "Terminal", "Cabinet", "Comment"],
            ["QF3 Cmd", "DPC", "XT3:1/XT3:2", "TB-1", "should not be imported"],
        ]
    )
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        type_column="Type",
        terminal_column="Terminal",
        selected_columns=["Name", "Type", "Terminal", "Cabinet"],
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="packed_selected.xlsx", metadata=meta)

    assert len(payload.signals) == 2
    first_row = payload.signals[0].signal_metadata["row"]
    second_row = payload.signals[1].signal_metadata["row"]

    assert set(first_row.keys()) == {"Name", "Type", "Terminal", "Cabinet"}
    assert set(second_row.keys()) == {"Name", "Type", "Terminal", "Cabinet"}
    assert "Comment" not in first_row
    assert "Comment" not in second_row
    assert first_row["Terminal"] == "XT3:1"
    assert second_row["Terminal"] == "XT3:2"
    sheet = payload.data["sheets"][0]
    assert sheet["headers"] == ["Name", "Type", "Terminal", "Cabinet"]
    assert "Comment" not in sheet["headers"]
    assert all(set(item.keys()) == {"Name", "Type", "Terminal", "Cabinet"} for item in sheet["rows"])


def test_parse_workbook_detects_header_row_after_preamble() -> None:
    raw = _build_workbook(
        [
            ["Signal list export", None, None],
            [None, None, None],
            ["Signal Name", "Vendor Type", "internal_type"],
            ["Pump Start", "DIGITAL OUTPUT", "do"],
            ["Pump Feedback", "DIGITAL INPUT", "di"],
        ]
    )
    meta = SignalImportMetaSchema(sheet_name="Signals", type_column="Vendor Type", internal_type_column="internal_type")

    payload = SignalSheetImportService.parse_workbook(raw, filename="preamble.xlsx", metadata=meta)

    assert payload.rows_count == 2
    assert payload.data["sheets"][0]["headers"] == ["Signal Name", "Vendor Type", "internal_type"]
    assert len(payload.signals) == 2
    assert {signal.io_direction for signal in payload.signals} == {"DI", "DO"}


def test_parse_workbook_honors_explicit_header_row_index() -> None:
    raw = _build_workbook(
        [
            ["Title", "Description", None],
            ["Signal Name", "Vendor Type", "internal_type"],
            ["Pump Start", "DIGITAL OUTPUT", "do"],
        ]
    )
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        header_row_index=1,
        type_column="Vendor Type",
        internal_type_column="internal_type",
    )

    payload = SignalSheetImportService.parse_workbook(raw, filename="explicit_header.xlsx", metadata=meta)

    assert payload.rows_count == 1
    assert payload.data["sheets"][0]["headers"] == ["Signal Name", "Vendor Type", "internal_type"]
    assert len(payload.signals) == 1
    assert payload.signals[0].name == "Pump Start"
