# ergo720 Copyright (c) 2023

cmake_minimum_required(VERSION 3.5)
project(nxbx)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${nxbx_BINARY_DIR}/bin")
endif()

set(NXBX_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

option(LIB86CPU_BUILD_SHARED_LIB "Build shared libraries" OFF)
add_subdirectory("import/lib86cpu")

if (CMAKE_GENERATOR MATCHES "Visual Studio")
 set(GENERATOR_IS_VS TRUE)
elseif (CMAKE_GENERATOR MATCHES "Unix Makefiles")
 set(GENERATOR_IS_UMAKE TRUE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
 set(COMPILER_IS_MSVC TRUE)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
 set(COMPILER_IS_GNU TRUE)
else ()
message(FATAL_ERROR "Unsupported compiler")
endif()

if (${GENERATOR_IS_UMAKE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
endif()

message("Building nxbx")
include_directories(
 ${NXBX_ROOT_DIR}/import/lib86cpu/include
 ${NXBX_ROOT_DIR}/import/simpleini
 ${NXBX_ROOT_DIR}/src/hw
)

set(HEADERS
 "${NXBX_ROOT_DIR}/src/clock.hpp"
 "${NXBX_ROOT_DIR}/src/console.hpp"
 "${NXBX_ROOT_DIR}/src/files.hpp"
 "${NXBX_ROOT_DIR}/src/io.hpp"
 "${NXBX_ROOT_DIR}/src/kernel.hpp"
 "${NXBX_ROOT_DIR}/src/logger.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx.hpp"
 "${NXBX_ROOT_DIR}/src/pe.hpp"
 "${NXBX_ROOT_DIR}/src/settings.hpp"
 "${NXBX_ROOT_DIR}/src/util.hpp"
 "${NXBX_ROOT_DIR}/src/xpartition.hpp"
 "${NXBX_ROOT_DIR}/src/hw/cmos.hpp"
 "${NXBX_ROOT_DIR}/src/hw/cpu.hpp"
 "${NXBX_ROOT_DIR}/src/hw/eeprom.hpp"
 "${NXBX_ROOT_DIR}/src/hw/machine.hpp"
 "${NXBX_ROOT_DIR}/src/hw/pci.hpp"
 "${NXBX_ROOT_DIR}/src/hw/pic.hpp"
 "${NXBX_ROOT_DIR}/src/hw/pit.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/vga.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/nv2a.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pbus.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pcrtc.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pfb.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pfifo.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pmc.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pramdac.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pramin.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/ptimer.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pvga.hpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pvideo.hpp"
)

set(SOURCES
 "${NXBX_ROOT_DIR}/src/clock.cpp"
 "${NXBX_ROOT_DIR}/src/files.cpp"
 "${NXBX_ROOT_DIR}/src/io.cpp"
 "${NXBX_ROOT_DIR}/src/kernel.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx.cpp"
 "${NXBX_ROOT_DIR}/src/main.cpp"
 "${NXBX_ROOT_DIR}/src/settings.cpp"
 "${NXBX_ROOT_DIR}/src/util.cpp"
 "${NXBX_ROOT_DIR}/src/xpartition.cpp"
 "${NXBX_ROOT_DIR}/src/hw/cmos.cpp"
 "${NXBX_ROOT_DIR}/src/hw/cpu.cpp"
 "${NXBX_ROOT_DIR}/src/hw/eeprom.cpp"
 "${NXBX_ROOT_DIR}/src/hw/pci.cpp"
 "${NXBX_ROOT_DIR}/src/hw/pic.cpp"
 "${NXBX_ROOT_DIR}/src/hw/pit.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/vga.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/nv2a.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pbus.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pcrtc.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pfb.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pfifo.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pmc.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pramdac.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pramin.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/ptimer.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pvga.cpp"
 "${NXBX_ROOT_DIR}/src/hw/video/gpu/pvideo.cpp"
)

source_group(TREE ${NXBX_ROOT_DIR} PREFIX header FILES ${HEADERS})
source_group(TREE ${NXBX_ROOT_DIR} PREFIX source FILES ${SOURCES})

if (${COMPILER_IS_MSVC})
set(CMAKE_CXX_FLAGS "/EHsc /Zc:preprocessor")
add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
endif()

add_executable(nxbx ${HEADERS} ${SOURCES})
target_link_libraries(nxbx PUBLIC cpu)

if (${COMPILER_IS_MSVC})
set_property(DIRECTORY "${NXBX_ROOT_DIR}" PROPERTY VS_STARTUP_PROJECT nxbx)
endif()
