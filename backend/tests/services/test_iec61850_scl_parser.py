from __future__ import annotations

from app.services.iec61850 import SCL_NORMALIZED_SCHEMA, parse_scl_source, scl_model_to_payload


SCD_FIXTURE = """<?xml version="1.0" encoding="UTF-8"?>
<scl:SCL xmlns:scl="http://www.iec.ch/61850/2003/SCL" version="2007" revision="B">
  <scl:Communication>
    <scl:SubNetwork name="StationBus" type="8-MMS">
      <scl:ConnectedAP iedName="IED1" apName="AP1">
        <scl:Address>
          <scl:P type="IP">192.168.14.1</scl:P>
          <scl:P type="OSI-TSEL">0001</scl:P>
        </scl:Address>
      </scl:ConnectedAP>
    </scl:SubNetwork>
  </scl:Communication>
  <scl:IED name="IED1" manufacturer="UnitLab" type="Simulator" configVersion="1">
    <scl:AccessPoint name="AP1">
      <scl:Server>
        <scl:LDevice inst="LD0">
          <scl:LN0 lnType="LLN0_TYPE">
            <scl:DataSet name="dsEvents">
              <scl:FCDA ldInst="LD0" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" />
              <scl:FCD ldInst="LD0" lnClass="PGGIO" lnInst="1" doName="Ind1" fc="ST" />
            </scl:DataSet>
            <scl:ReportControl name="brcbEvents" buffered="true" rptID="IED1LD0/LLN0.BR.Events" datSet="dsEvents" confRev="7" indexed="false" bufTime="100" intgPd="1000">
              <scl:TrgOps dchg="true" qchg="true" dupd="false" period="false" gi="true" />
              <scl:OptFields seqNum="true" timeStamp="true" reasonCode="true" dataSet="true" dataRef="true" bufOvfl="true" entryID="true" configRef="true" />
            </scl:ReportControl>
          </scl:LN0>
          <scl:LN lnClass="XCBR" inst="1" lnType="XCBR_TYPE" />
          <scl:LN lnClass="PGGIO" inst="1" lnType="PGGIO_TYPE" />
        </scl:LDevice>
      </scl:Server>
    </scl:AccessPoint>
  </scl:IED>
</scl:SCL>
"""


def test_parse_scl_source_normalizes_server_model() -> None:
    model = parse_scl_source(file_name="test.scd", content_hash="sha256:test", xml_text=SCD_FIXTURE)

    assert model.schema == SCL_NORMALIZED_SCHEMA
    assert model.scl_version == "2007"
    assert model.scl_revision == "B"
    assert [diagnostic for diagnostic in model.diagnostics if diagnostic.severity == "error"] == []

    ied = model.ieds[0]
    assert ied.name == "IED1"
    assert ied.manufacturer == "UnitLab"

    access_point = ied.access_points[0]
    assert access_point.name == "AP1"
    assert access_point.connected_access_points[0].address[0].type == "IP"
    assert access_point.connected_access_points[0].address[0].value == "192.168.14.1"

    logical_device = access_point.logical_devices[0]
    assert logical_device.inst == "LD0"
    assert [node.name for node in logical_device.logical_nodes] == ["LLN0", "XCBR1", "PGGIO1"]

    lln0 = logical_device.logical_nodes[0]
    data_set = lln0.data_sets[0]
    assert data_set.reference == "LD0/LLN0$dsEvents"
    assert [member.reference for member in data_set.members] == [
        "LD0/XCBR1$ST$Pos$stVal",
        "LD0/PGGIO1$ST$Ind1",
    ]

    report = lln0.report_controls[0]
    assert report.reference == "LD0/LLN0.BR.brcbEvents"
    assert report.report_kind == "buffered"
    assert report.rpt_id == "IED1LD0/LLN0.BR.Events"
    assert report.data_set_ref == "LD0/LLN0$dsEvents"
    assert report.conf_rev == 7
    assert report.indexed is False
    assert report.buffer_time_ms == 100
    assert report.integrity_period_ms == 1000
    assert report.trigger_options.data_change is True
    assert report.trigger_options.quality_change is True
    assert report.trigger_options.data_update is False
    assert report.trigger_options.integrity is False
    assert report.trigger_options.general_interrogation is True
    assert report.optional_fields.sequence_number is True
    assert report.optional_fields.config_revision is True


def test_scl_model_to_payload_uses_backend_contract_keys() -> None:
    model = parse_scl_source(file_name="test.scd", content_hash="sha256:test", xml_text=SCD_FIXTURE)
    payload = scl_model_to_payload(model)

    assert payload["schema"] == SCL_NORMALIZED_SCHEMA
    assert payload["ieds"][0]["accessPoints"][0]["logicalDevices"][0]["logicalNodes"][0]["reportControls"][0]["dataSetRef"] == "LD0/LLN0$dsEvents"
