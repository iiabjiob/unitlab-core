from __future__ import annotations

from dataclasses import dataclass
from hashlib import sha256
from typing import Any

from .ied_simulator_fixture import build_ied_simulator_fixture_from_scl_model, ied_simulator_fixture_to_payload
from .scl_parser import Iec61850SclModel, parse_scl_source, scl_model_to_payload


@dataclass(frozen=True, slots=True)
class Iec61850SclImportResult:
    model: Iec61850SclModel
    model_payload: dict[str, Any]
    simulator_fixture_payload: dict[str, Any]


def import_scl_source(
    *,
    file_name: str,
    xml_text: str,
    selected_ied_name: str | None = None,
) -> Iec61850SclImportResult:
    content_hash = "sha256:" + sha256(xml_text.encode("utf-8")).hexdigest()
    model = parse_scl_source(file_name=file_name, content_hash=content_hash, xml_text=xml_text)
    fixture = build_ied_simulator_fixture_from_scl_model(model, selected_ied_name=selected_ied_name)
    return Iec61850SclImportResult(
        model=model,
        model_payload=scl_model_to_payload(model),
        simulator_fixture_payload=ied_simulator_fixture_to_payload(fixture),
    )


def scl_import_result_to_payload(result: Iec61850SclImportResult) -> dict[str, Any]:
    error_count = sum(1 for diagnostic in result.model.diagnostics if diagnostic.severity == "error")
    warning_count = sum(1 for diagnostic in result.model.diagnostics if diagnostic.severity == "warning")
    return {
        "model": result.model_payload,
        "simulatorFixture": result.simulator_fixture_payload,
        "summary": {
            "iedCount": len(result.model.ieds),
            "deviceCount": len(result.simulator_fixture_payload.get("devices", [])),
            "errorCount": error_count,
            "warningCount": warning_count,
        },
    }
