from __future__ import annotations

import asyncio
from tempfile import SpooledTemporaryFile

import pytest
from starlette.datastructures import UploadFile

from app.api.v1.iec61850.router import import_scl_model

SCD_FIXTURE = """<?xml version="1.0" encoding="UTF-8"?>
<scl:SCL xmlns:scl="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <scl:IED name="IED1" manufacturer="UnitLab" type="Simulator" configVersion="1">
    <scl:AccessPoint name="AP1">
      <scl:Server>
        <scl:LDevice inst="LD0">
          <scl:LN0 lnType="LLN0_TYPE">
            <scl:DataSet name="dsEvents">
              <scl:FCDA ldInst="LD0" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" />
            </scl:DataSet>
            <scl:ReportControl name="brcbEvents" buffered="true" datSet="dsEvents" confRev="7" />
          </scl:LN0>
          <scl:LN lnClass="XCBR" inst="1" lnType="XCBR_TYPE" />
        </scl:LDevice>
      </scl:Server>
    </scl:AccessPoint>
  </scl:IED>
</scl:SCL>
"""


def _upload_file(name: str, payload: bytes) -> UploadFile:
    file = SpooledTemporaryFile()
    file.write(payload)
    file.seek(0)
    return UploadFile(file=file, filename=name)


def test_import_scl_model_endpoint_returns_normalized_model_and_fixture() -> None:
    payload = asyncio.run(import_scl_model(
        file=_upload_file("test.scd", SCD_FIXTURE.encode("utf-8")),
        selected_ied_name="IED1",
    ))

    assert payload["summary"] == {
        "iedCount": 1,
        "deviceCount": 1,
        "errorCount": 0,
        "warningCount": 0,
    }
    assert payload["model"]["ieds"][0]["name"] == "IED1"
    assert payload["simulatorFixture"]["schema"] == "unitlab.iec61850.ied-simulator-fixture.v1"
    assert payload["simulatorFixture"]["devices"][0]["dataSets"][0]["reference"] == "IED1/AP1/LD0/LLN0.dsEvents"


def test_import_scl_model_endpoint_rejects_non_utf8_upload() -> None:
    with pytest.raises(Exception) as error:
        asyncio.run(import_scl_model(file=_upload_file("bad.scd", b"\xff\xfe\x00")))

    assert getattr(error.value, "status_code", None) == 400
    assert error.value.detail["code"] == "SCL_ENCODING_UNSUPPORTED"
