cmake_minimum_required(VERSION 3.16)

project(unitlab_iec61850_ied_sim C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(UNITLAB_IEC61850_SIM_WITH_LIBIEC61850 "Link libIEC61850 for the internal IEC 61850 IED simulator" OFF)

function(unitlab_iec61850_sim_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

add_library(unitlab-iec61850-mms STATIC
  src/unitlab_mms_types.c
  src/unitlab_mms_semantic_pdu.c
  src/unitlab_mms_wire_semantic_bridge.c
  src/unitlab_mms_runtime_bridge.c
  src/unitlab_mms_server_runtime.c
  src/unitlab_mms_core.c
  src/unitlab_iec61850_report_runtime.c
  src/wire/ber/unitlab_mms_ber_tag.c
  src/wire/ber/unitlab_mms_ber_length.c
  src/wire/ber/unitlab_mms_ber_reader.c
  src/wire/ber/unitlab_mms_ber_writer.c
  src/wire/acse/unitlab_mms_acse.c
  src/wire/session/unitlab_mms_session_spdu.c
  src/wire/presentation/unitlab_mms_presentation.c
  src/wire/transport/unitlab_mms_transport_frame.c
  src/wire/transport/unitlab_mms_wire_association_fixture.c
  src/wire/orchestration/unitlab_mms_wire_builder.c
  src/wire/mms/unitlab_mms_pdu.c
  src/wire/iso/unitlab_mms_cotp.c
  src/wire/iso/unitlab_mms_tpkt.c
)
target_include_directories(unitlab-iec61850-mms PUBLIC src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-mms)
add_library(unitlab::iec61850_mms ALIAS unitlab-iec61850-mms)

add_executable(unitlab-iec61850-ied-sim
  src/client_probe.c
  src/fixture_parser.c
  src/libiec61850_model_builder.c
  src/main.c
  src/native_wire_server.c
  src/model_loader.c
  src/model_plan.c
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

add_executable(unitlab-iec61850-ied-fixture-parser-test
  src/fixture_parser.c
  tests/fixture_parser_test.c
)
target_include_directories(unitlab-iec61850-ied-fixture-parser-test PRIVATE src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-fixture-parser-test)

add_test(
  NAME unitlab-iec61850-ied-fixture-parser
  COMMAND unitlab-iec61850-ied-fixture-parser-test
)

add_executable(unitlab-iec61850-ied-model-plan-test
  src/model_plan.c
  tests/model_plan_test.c
)
target_include_directories(unitlab-iec61850-ied-model-plan-test PRIVATE src)
unitlab_iec61850_sim_warnings(unitlab-iec61850-ied-model-plan-test)

add_test(
  NAME unitlab-iec61850-ied-model-plan
  COMMAND unitlab-iec61850-ied-model-plan-test
)

add_executable(unitlab-iec61850-ied-model-loader-test
  src/libiec61850_model_builder.c
  src/model_loader.c
  src/model_plan.c
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

add_executable(unitlab-iec61850-wire-foundation-test
  tests/unitlab_mms_wire_primitives_test.c
)
target_link_libraries(unitlab-iec61850-wire-foundation-test PRIVATE unitlab-iec61850-mms)
unitlab_iec61850_sim_warnings(unitlab-iec61850-wire-foundation-test)

add_test(
  NAME unitlab-iec61850-wire-foundation
  COMMAND unitlab-iec61850-wire-foundation-test
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
    src/client_probe.c
    src/libiec61850_model_builder.c
    src/model_loader.c
    src/model_plan.c
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
