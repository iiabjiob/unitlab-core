cmake_minimum_required(VERSION 3.16)

project(unitlab_iec61850_ied_sim C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(UNITLAB_IEC61850_SIM_WITH_LIBIEC61850 "Link libIEC61850 for the internal IEC 61850 IED simulator" OFF)

add_executable(unitlab-iec61850-ied-sim src/main.c)

if(MSVC)
  target_compile_options(unitlab-iec61850-ied-sim PRIVATE /W4)
else()
  target_compile_options(unitlab-iec61850-ied-sim PRIVATE -Wall -Wextra -Wpedantic)
endif()

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
