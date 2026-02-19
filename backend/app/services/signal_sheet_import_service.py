from __future__ import annotations

import io
import math
import re
from dataclasses import dataclass
from datetime import date, datetime
from typing import Any

import xlrd
from openpyxl import load_workbook

from app.schemas.signal_import_schema import SignalImportMetaSchema


_INTERNAL_TYPE_TO_DIRECTION: dict[str, str] = {
    "di": "DI",
    "do": "DO",
    "ai": "AI",
    "ao": "AO",
}


@dataclass(frozen=True)
class ImportedSignalProjection:
    key: str
    name: str
    io_direction: str
    category: str | None
    signal_metadata: dict[str, Any]


@dataclass(frozen=True)
class ImportedSheetPayload:
    data: dict[str, Any]
    rows_count: int
    signals: list[ImportedSignalProjection]


class SignalSheetImportService:
    """Parse workbook bytes into normalized sheet payload + optional signal projection."""

    @staticmethod
    def parse_workbook(
        file_bytes: bytes,
        *,
        filename: str | None,
        metadata: SignalImportMetaSchema | None,
    ) -> ImportedSheetPayload:
        matrices = SignalSheetImportService._read_workbook(file_bytes, filename)
        sheets: list[dict[str, Any]] = []
        total_rows = 0

        for sheet_index, (sheet_name, matrix) in enumerate(matrices):
            headers = SignalSheetImportService._normalize_headers(matrix[0] if matrix else [])
            rows = SignalSheetImportService._rows_to_objects(matrix[1:], headers)
            total_rows += len(rows)
            sheets.append(
                {
                    "name": sheet_name,
                    "index": sheet_index,
                    "headers": headers,
                    "rows_count": len(rows),
                    "rows": rows,
                }
            )

        if not sheets:
            sheets.append(
                {
                    "name": "Sheet 1",
                    "index": 0,
                    "headers": [],
                    "rows_count": 0,
                    "rows": [],
                }
            )

        default_sheet_index = SignalSheetImportService._select_default_sheet_index(sheets, metadata)

        sheet_data = {
            "version": 2,
            "sheet_count": len(sheets),
            "default_sheet_index": default_sheet_index,
            "sheets": sheets,
        }

        projected_signals = SignalSheetImportService._project_signals(
            sheets=sheets,
            default_sheet_index=default_sheet_index,
            filename=filename,
            metadata=metadata,
        )

        return ImportedSheetPayload(
            data=sheet_data,
            rows_count=total_rows,
            signals=projected_signals,
        )

    @staticmethod
    def _read_workbook(file_bytes: bytes, filename: str | None) -> list[tuple[str, list[list[Any]]]]:
        extension = (filename or "").lower().split(".")[-1] if filename and "." in filename else ""

        if extension == "xls":
            return SignalSheetImportService._read_xls(file_bytes)

        try:
            return SignalSheetImportService._read_xlsx(file_bytes)
        except Exception:
            return SignalSheetImportService._read_xls(file_bytes)

    @staticmethod
    def _read_xlsx(file_bytes: bytes) -> list[tuple[str, list[list[Any]]]]:
        workbook = load_workbook(io.BytesIO(file_bytes), data_only=True, read_only=True)
        result: list[tuple[str, list[list[Any]]]] = []
        for ws in workbook.worksheets:
            matrix: list[list[Any]] = []
            for row in ws.iter_rows(values_only=True):
                matrix.append(list(row))
            result.append((ws.title or "Sheet", matrix))
        return result

    @staticmethod
    def _read_xls(file_bytes: bytes) -> list[tuple[str, list[list[Any]]]]:
        workbook = xlrd.open_workbook(file_contents=file_bytes)
        result: list[tuple[str, list[list[Any]]]] = []
        for sheet in workbook.sheets():
            matrix: list[list[Any]] = []
            for row_index in range(sheet.nrows):
                matrix.append(sheet.row_values(row_index))
            result.append((sheet.name or "Sheet", matrix))
        return result

    @staticmethod
    def _normalize_headers(raw_headers: list[Any]) -> list[str]:
        normalized: list[str] = []
        seen: dict[str, int] = {}

        for index, raw in enumerate(raw_headers):
            value = SignalSheetImportService._stringify_cell(raw).strip()
            base = value if value else f"column_{index + 1}"
            suffix = seen.get(base, 0)
            seen[base] = suffix + 1
            header = base if suffix == 0 else f"{base}_{suffix + 1}"
            normalized.append(header)

        return normalized

    @staticmethod
    def _rows_to_objects(rows: list[list[Any]], headers: list[str]) -> list[dict[str, Any]]:
        if not headers:
            return []

        payload: list[dict[str, Any]] = []
        for row in rows:
            item: dict[str, Any] = {}
            non_empty = False
            for col_idx, header in enumerate(headers):
                value = SignalSheetImportService._normalize_cell(row[col_idx] if col_idx < len(row) else None)
                item[header] = value
                if value not in (None, ""):
                    non_empty = True
            if non_empty:
                payload.append(item)
        return payload

    @staticmethod
    def _normalize_cell(value: Any) -> Any:
        if value is None:
            return None
        if isinstance(value, datetime):
            return value.isoformat()
        if isinstance(value, date):
            return value.isoformat()
        if isinstance(value, bytes):
            try:
                return value.decode("utf-8")
            except UnicodeDecodeError:
                return value.decode("latin-1", errors="ignore")
        if isinstance(value, float) and math.isnan(value):
            return None
        return value

    @staticmethod
    def _select_default_sheet_index(
        sheets: list[dict[str, Any]],
        metadata: SignalImportMetaSchema | None,
    ) -> int:
        if metadata and metadata.sheet_name:
            for sheet in sheets:
                if sheet["name"] == metadata.sheet_name:
                    return int(sheet["index"])

        for sheet in sheets:
            if int(sheet.get("rows_count") or 0) > 0:
                return int(sheet["index"])

        return 0

    @staticmethod
    def _project_signals(
        *,
        sheets: list[dict[str, Any]],
        default_sheet_index: int,
        filename: str | None,
        metadata: SignalImportMetaSchema | None,
    ) -> list[ImportedSignalProjection]:
        selected = next((sheet for sheet in sheets if sheet["index"] == default_sheet_index), None)
        if not selected:
            return []

        rows = selected.get("rows") or []
        if not rows:
            return []

        headers = selected.get("headers") or []
        signal_name_column = SignalSheetImportService._pick_signal_name_column(headers)
        internal_type_column = SignalSheetImportService._pick_internal_type_column(headers, metadata)
        type_column = metadata.type_column if metadata and metadata.type_column in headers else None
        type_mapping = {
            str(key).strip().lower(): str(value).strip().lower()
            for key, value in (metadata.type_mapping if metadata else {}).items()
            if str(key).strip() and str(value).strip()
        }

        projections: list[ImportedSignalProjection] = []
        seen_keys: set[str] = set()

        for row_index, row in enumerate(rows):
            internal_type = SignalSheetImportService._resolve_internal_type(
                row=row,
                internal_type_column=internal_type_column,
                type_column=type_column,
                type_mapping=type_mapping,
            )
            direction = _INTERNAL_TYPE_TO_DIRECTION.get(internal_type)
            if not direction:
                continue

            display_name = (
                SignalSheetImportService._stringify_cell(row.get(signal_name_column)).strip()
                if signal_name_column
                else ""
            )
            if not display_name:
                display_name = f"Signal {row_index + 1}"

            base_key = SignalSheetImportService._slugify(display_name)
            key = base_key
            suffix = 2
            while key in seen_keys:
                key = f"{base_key}_{suffix}"
                suffix += 1
            seen_keys.add(key)

            category = None
            if type_column:
                category_raw = row.get(type_column)
                category_str = SignalSheetImportService._stringify_cell(category_raw).strip()
                category = category_str or None

            signal_metadata = {
                "source": "signal_sheet_import",
                "source_filename": filename,
                "sheet_name": selected.get("name"),
                "sheet_index": selected.get("index"),
                "row_index": row_index,
                "row": row,
            }

            projections.append(
                ImportedSignalProjection(
                    key=key,
                    name=display_name,
                    io_direction=direction,
                    category=category,
                    signal_metadata=signal_metadata,
                )
            )

        return projections

    @staticmethod
    def _pick_signal_name_column(headers: list[str]) -> str | None:
        heuristics = ("hmi", "name", "signal", "description")
        for header in headers:
            lowered = header.lower()
            if any(token in lowered for token in heuristics):
                return header
        return headers[0] if headers else None

    @staticmethod
    def _pick_internal_type_column(headers: list[str], metadata: SignalImportMetaSchema | None) -> str | None:
        if metadata and metadata.internal_type_column and metadata.internal_type_column in headers:
            return metadata.internal_type_column

        heuristics = ("internal_type", "internal type", "type", "тип")
        for header in headers:
            lowered = header.lower()
            if any(token in lowered for token in heuristics):
                return header
        return None

    @staticmethod
    def _resolve_internal_type(
        *,
        row: dict[str, Any],
        internal_type_column: str | None,
        type_column: str | None,
        type_mapping: dict[str, str],
    ) -> str:
        if internal_type_column:
            raw = SignalSheetImportService._stringify_cell(row.get(internal_type_column)).strip().lower()
            if raw in _INTERNAL_TYPE_TO_DIRECTION:
                return raw

        if type_column:
            raw_type = SignalSheetImportService._stringify_cell(row.get(type_column)).strip().lower()
            mapped = type_mapping.get(raw_type)
            if mapped in _INTERNAL_TYPE_TO_DIRECTION:
                return mapped

        return ""

    @staticmethod
    def _slugify(value: str) -> str:
        lowered = value.strip().lower()
        slug = re.sub(r"[^a-z0-9]+", "_", lowered).strip("_")
        return slug or "signal"

    @staticmethod
    def _stringify_cell(value: Any) -> str:
        if value is None:
            return ""
        if isinstance(value, str):
            return value
        if isinstance(value, bool):
            return "true" if value else "false"
        return str(value)
