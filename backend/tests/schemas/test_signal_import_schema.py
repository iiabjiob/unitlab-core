from __future__ import annotations

from app.schemas.signal_import_schema import (
    SignalImportMetaSchema,
    SignalImportVerificationColumnHintSchema,
    SignalImportVerificationSchema,
)


def test_signal_import_meta_schema_preserves_verification_hints() -> None:
    meta = SignalImportMetaSchema(
        sheet_name="Signals",
        verification=SignalImportVerificationSchema(
            enabled=True,
            transport_host_column="MMS Host",
            iec61850_address_column="61850 Address",
            transport_reference_column_hint=SignalImportVerificationColumnHintSchema(
                column="MMS Endpoint",
                confidence="likely",
                reason="header contains endpoint keyword",
                sample_values=["10.0.0.12:102"],
            ),
            iec61850_address_column_hint=SignalImportVerificationColumnHintSchema(
                column="61850 Address",
                confidence="exact",
                reason="header contains IEC 61850 address keyword",
                sample_values=["IED-A/P1/LLN0.brA"],
            ),
            notes=["verification-ready"],
        ),
    )

    payload = meta.model_dump()

    assert payload["verification"]["enabled"] is True
    assert payload["verification"]["transport_host_column"] == "MMS Host"
    assert payload["verification"]["iec61850_address_column"] == "61850 Address"
    assert payload["verification"]["transport_reference_column_hint"]["column"] == "MMS Endpoint"
    assert payload["verification"]["iec61850_address_column_hint"]["confidence"] == "exact"
    assert payload["verification"]["notes"] == ["verification-ready"]
