cmake_minimum_required(VERSION 3.16)

project(unitlab_iec61850_ied_sim C CXX)

find_package(Threads REQUIRED)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UNITLAB_IEC61850_SIM_WITH_LIBIEC61850 "Link libIEC61850 for the internal IEC 61850 IED simulator" OFF)

function(unitlab_iec61850_sim_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_library(unitlab-iec61850-mms STATIC
  src/protocols/mms/unitlab_mms_types.c
  src/protocols/mms/unitlab_mms_semantic_pdu.c
  src/protocols/mms/unitlab_mms_wire_semantic_bridge.c
  src/protocols/mms/unitlab_mms_runtime_bridge.c
  src/server/unitlab_mms_server_runtime.c
  src/server/services/unitlab_mms_server_browse_service.c
  src/server/services/unitlab_mms_server_read_service.c
  src/server/services/unitlab_mms_server_report_service.c
  src/server/services/unitlab_mms_server_write_service.c
  src/protocols/mms/unitlab_mms_core.c
  src/report/unitlab_iec61850_report_runtime.c
  src/model/model_plan.c
  src/wire/ber/unitlab_mms_ber_tag.c
  src/wire/ber/unitlab_mms_ber_length.c
  src/wire/ber/unitlab_mms_ber_reader.c
  src/wire/ber/unitlab_mms_ber_writer.c
  src/wire/acse/unitlab_mms_acse.c
  src/wire/session/unitlab_mms_session_spdu.c
  src/wire/presentation/unitlab_mms_presentation.c
  src/wire/transport/unitlab_mms_transport_frame.c
  src/wire/orchestration/unitlab_mms_association_frame.c
  src/wire/orchestration/unitlab_mms_wire_builder_common.c
  src/wire/orchestration/unitlab_mms_association_builder.c
  src/wire/orchestration/unitlab_mms_data_access_builder.c
  src/wire/orchestration/unitlab_mms_report_builder.c
  src/wire/orchestration/unitlab_mms_live_wire_probe.c
  src/wire/orchestration/unitlab_mms_wire_builder.c
  src/wire/mms/unitlab_mms_pdu.c
  src/wire/iso/unitlab_mms_cotp.c
  src/wire/iso/unitlab_mms_tpkt.c
)
target_include_directories(unitlab-iec61850-mms PUBLIC src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms)
add_library(unitlab::iec61850_mms ALIAS unitlab-iec61850-mms)

add_library(unitlab-iec61850-scl-compiler STATIC
  third_party/pugixml/pugixml.cpp
  src/scl_compiler/scl_dom.cpp
  src/scl_compiler/unitlab_scl_compiler.cpp
)
target_include_directories(unitlab-iec61850-scl-compiler PUBLIC src PRIVATE third_party/pugixml third_party/nlohmann)
unitlab_iec61850_sim_warnings(unitlab-iec61850-scl-compiler)
add_library(unitlab::iec61850_scl_compiler ALIAS unitlab-iec61850-scl-compiler)

add_executable(unitlab-iec61850-ied-sim
  src/app/client_probe.c
  src/fixture/fixture_parser.c
  src/model/libiec61850_model_builder.c
  src/app/main.c
  src/server/native_wire_client.c
  src/server/native_wire_server.c
  src/model/model_loader.c
)
target_link_libraries(unitlab-iec61850-ied-sim PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-sim)

if(UNITLAB_IEC61850_SIM_WITH_LIBIEC61850)
  find_path(LIBIEC61850_INCLUDE_DIR iec61850_server.h)
  find_library(LIBIEC61850_LIBRARY NAMES iec61850)

  if(NOT LIBIEC61850_INCLUDE_DIR OR NOT LIBIEC61850_LIBRARY)
    message(FATAL_ERROR "libIEC61850 headers/library were not found. Set LIBIEC61850_INCLUDE_DIR and LIBIEC61850_LIBRARY.")
  endif()

  target_compile_definitions(unitlab-iec61850-ied-sim PRIVATE UNITLAB_WITH_LIBIEC61850=1)
  target_include_directories(unitlab-iec61850-ied-sim PRIVATE "${LIBIEC61850_INCLUDE_DIR}")
  target_link_libraries(unitlab-iec61850-ied-sim PRIVATE "${LIBIEC61850_LIBRARY}")
endif()

enable_testing()

add_executable(unitlab-iec61850-scl-compiler-api-test
  tests/scl_compiler/scl_compiler_api_test.c
)
target_link_libraries(unitlab-iec61850-scl-compiler-api-test PRIVATE unitlab-iec61850-scl-compiler)
unitlab_iec61850_sim_warnings(unitlab-iec61850-scl-compiler-api-test)

add_test(
  NAME unitlab-iec61850-scl-compiler-api
  COMMAND unitlab-iec61850-scl-compiler-api-test
)

add_executable(unitlab-iec61850-ied-fixture-parser-test
  src/fixture/fixture_parser.c
  tests/fixture_parser_test.c
)
target_include_directories(unitlab-iec61850-ied-fixture-parser-test PRIVATE src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-fixture-parser-test)

add_test(
  NAME unitlab-iec61850-ied-fixture-parser
  COMMAND unitlab-iec61850-ied-fixture-parser-test
)

add_executable(unitlab-iec61850-ied-model-plan-test
  src/model/model_plan.c
  tests/model_plan_test.c
)
target_include_directories(unitlab-iec61850-ied-model-plan-test PRIVATE src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-model-plan-test)

add_test(
  NAME unitlab-iec61850-ied-model-plan
  COMMAND unitlab-iec61850-ied-model-plan-test
)

add_executable(unitlab-iec61850-ied-model-loader-test
  src/model/libiec61850_model_builder.c
  src/model/model_loader.c
  src/model/model_plan.c
  tests/model_loader_test.c
)
target_include_directories(unitlab-iec61850-ied-model-loader-test PRIVATE src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-model-loader-test)

if(UNITLAB_IEC61850_SIM_WITH_LIBIEC61850)
  target_compile_definitions(unitlab-iec61850-ied-model-loader-test PRIVATE UNITLAB_WITH_LIBIEC61850=1)
  target_include_directories(unitlab-iec61850-ied-model-loader-test PRIVATE "${LIBIEC61850_INCLUDE_DIR}")
  target_link_libraries(unitlab-iec61850-ied-model-loader-test PRIVATE "${LIBIEC61850_LIBRARY}")
endif()

add_test(
  NAME unitlab-iec61850-ied-model-loader
  COMMAND unitlab-iec61850-ied-model-loader-test
)

add_executable(unitlab-iec61850-mms-core-test
  tests/unitlab_mms_core_test.c
)
target_link_libraries(unitlab-iec61850-mms-core-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms-core-test)

add_test(
  NAME unitlab-iec61850-mms-core
  COMMAND unitlab-iec61850-mms-core-test
)

add_executable(unitlab-iec61850-mms-semantic-test
  tests/unitlab_mms_semantic_pdu_test.c
)
target_link_libraries(unitlab-iec61850-mms-semantic-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms-semantic-test)

add_test(
  NAME unitlab-iec61850-mms-semantic
  COMMAND unitlab-iec61850-mms-semantic-test
)

add_executable(unitlab-iec61850-mms-runtime-bridge-test
  tests/unitlab_mms_runtime_bridge_test.c
)
target_link_libraries(unitlab-iec61850-mms-runtime-bridge-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms-runtime-bridge-test)

add_test(
  NAME unitlab-iec61850-mms-runtime-bridge
  COMMAND unitlab-iec61850-mms-runtime-bridge-test
)

add_executable(unitlab-iec61850-wire-ber-test
  tests/wire/test_ber.c
)
target_link_libraries(unitlab-iec61850-wire-ber-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-ber-test)
add_test(NAME unitlab-iec61850-wire-ber COMMAND unitlab-iec61850-wire-ber-test)

add_executable(unitlab-iec61850-wire-tpkt-test
  tests/wire/test_tpkt.c
)
target_link_libraries(unitlab-iec61850-wire-tpkt-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-tpkt-test)
add_test(NAME unitlab-iec61850-wire-tpkt COMMAND unitlab-iec61850-wire-tpkt-test)

add_executable(unitlab-iec61850-wire-cotp-test
  tests/wire/test_cotp.c
)
target_link_libraries(unitlab-iec61850-wire-cotp-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-cotp-test)
add_test(NAME unitlab-iec61850-wire-cotp COMMAND unitlab-iec61850-wire-cotp-test)

add_executable(unitlab-iec61850-wire-transport-frame-test
  tests/wire/test_transport_frame.c
)
target_link_libraries(unitlab-iec61850-wire-transport-frame-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-transport-frame-test)
add_test(NAME unitlab-iec61850-wire-transport-frame COMMAND unitlab-iec61850-wire-transport-frame-test)

add_executable(unitlab-iec61850-wire-session-test
  tests/wire/test_session_spdu.c
)
target_link_libraries(unitlab-iec61850-wire-session-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-session-test)
add_test(NAME unitlab-iec61850-wire-session COMMAND unitlab-iec61850-wire-session-test)

add_executable(unitlab-iec61850-wire-presentation-test
  tests/wire/test_presentation.c
)
target_link_libraries(unitlab-iec61850-wire-presentation-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-presentation-test)
add_test(NAME unitlab-iec61850-wire-presentation COMMAND unitlab-iec61850-wire-presentation-test)

add_executable(unitlab-iec61850-wire-acse-test
  tests/wire/test_acse.c
)
target_link_libraries(unitlab-iec61850-wire-acse-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-acse-test)
add_test(NAME unitlab-iec61850-wire-acse COMMAND unitlab-iec61850-wire-acse-test)

add_executable(unitlab-iec61850-wire-mms-pdu-test
  tests/wire/test_mms_pdu.c
)
target_link_libraries(unitlab-iec61850-wire-mms-pdu-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-mms-pdu-test)
add_test(NAME unitlab-iec61850-wire-mms-pdu COMMAND unitlab-iec61850-wire-mms-pdu-test)

add_executable(unitlab-iec61850-wire-association-frame-test
  tests/wire/test_association_frame.c
)
target_link_libraries(unitlab-iec61850-wire-association-frame-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-association-frame-test)
add_test(NAME unitlab-iec61850-wire-association-frame COMMAND unitlab-iec61850-wire-association-frame-test)

add_executable(unitlab-iec61850-wire-builder-test
  tests/wire/test_wire_builder.c
)
target_link_libraries(unitlab-iec61850-wire-builder-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-builder-test)
add_test(NAME unitlab-iec61850-wire-builder COMMAND unitlab-iec61850-wire-builder-test)

add_executable(unitlab-iec61850-wire-stack-ladder-test
  tests/wire/test_stack_ladder.c
)
target_link_libraries(unitlab-iec61850-wire-stack-ladder-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-stack-ladder-test)

add_test(
  NAME unitlab-iec61850-wire-stack-ladder
  COMMAND unitlab-iec61850-wire-stack-ladder-test
)

add_executable(unitlab-iec61850-native-wire-server-test
  src/server/native_wire_server.c
  tests/native_wire_server_smoke_test.c
)
target_include_directories(unitlab-iec61850-native-wire-server-test PRIVATE src)
target_link_libraries(unitlab-iec61850-native-wire-server-test PRIVATE unitlab-iec61850-mms Threads::Threads)
unitlab_iec61850_sim_warnings(unitlab-iec61850-native-wire-server-test)

add_test(
  NAME unitlab-iec61850-native-wire-server
  COMMAND unitlab-iec61850-native-wire-server-test
)

add_executable(unitlab-iec61850-mms-public-api-test
  tests/unitlab_mms_public_api_test.c
)
target_link_libraries(unitlab-iec61850-mms-public-api-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms-public-api-test)

add_test(
  NAME unitlab-iec61850-mms-public-api
  COMMAND unitlab-iec61850-mms-public-api-test
)

add_executable(unitlab-iec61850-mms-server-runtime-test
  tests/unitlab_mms_server_runtime_test.c
)
target_link_libraries(unitlab-iec61850-mms-server-runtime-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms-server-runtime-test)

add_test(
  NAME unitlab-iec61850-mms-server-runtime
  COMMAND unitlab-iec61850-mms-server-runtime-test
)

if(UNITLAB_IEC61850_SIM_WITH_LIBIEC61850)
  add_executable(unitlab-iec61850-ied-linked-client-smoke-test
    src/app/client_probe.c
    src/model/libiec61850_model_builder.c
    src/model/model_loader.c
    src/model/model_plan.c
    tests/linked_client_smoke_test.c
  )
  target_compile_definitions(unitlab-iec61850-ied-linked-client-smoke-test PRIVATE UNITLAB_WITH_LIBIEC61850=1)
  target_include_directories(unitlab-iec61850-ied-linked-client-smoke-test PRIVATE src "${LIBIEC61850_INCLUDE_DIR}")
  target_link_libraries(unitlab-iec61850-ied-linked-client-smoke-test PRIVATE "${LIBIEC61850_LIBRARY}")
  unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-linked-client-smoke-test)

  add_test(
    NAME unitlab-iec61850-ied-linked-client-smoke
    COMMAND unitlab-iec61850-ied-linked-client-smoke-test
  )
endif()
