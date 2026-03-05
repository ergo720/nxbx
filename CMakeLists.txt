# ergo720 Copyright (c) 2023

# Suppress build failures caused by deprecation warnings treated as errors. CI seems to be using "-Werror=deprecated" which causes the issue
set(CMAKE_ERROR_DEPRECATED FALSE CACHE BOOL "" FORCE)

cmake_minimum_required(VERSION 3.5)
project(nxbx)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${nxbx_BINARY_DIR}/bin")
endif()

set(NXBX_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

find_package(Qt6 6.10.2 REQUIRED COMPONENTS Core Gui Widgets)
if(${GENERATOR_IS_VS})
find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
if(${WINDEPLOYQT_EXECUTABLE} STREQUAL "")
 message(FATAL_ERROR "windeployqt was not found")
endif()
endif()

find_package(Git)
if(Git_FOUND)
 execute_process(
  COMMAND git ls-remote https://github.com/ergo720/nboxkrnl HEAD
  OUTPUT_VARIABLE _NBOXKRNL_HEAD_REF
  OUTPUT_STRIP_TRAILING_WHITESPACE
 )
 configure_file(
  "${NXBX_ROOT_DIR}/src/nxbx/kernel_head_ref.hpp.in" "${NXBX_ROOT_DIR}/src/nxbx/kernel_head_ref.hpp" @ONLY
  NEWLINE_STYLE LF
 )
else()
 message(FATAL_ERROR "Git was not found")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(LIB86CPU_BUILD_SHARED_LIB "Build shared libraries" OFF)
option(LIB86CPU_XBOX_BUILD "Building Xbox version" ON)
add_subdirectory("import/lib86cpu")

if(CMAKE_GENERATOR MATCHES "Visual Studio")
 set(GENERATOR_IS_VS TRUE)
elseif(CMAKE_GENERATOR MATCHES "Unix Makefiles")
 set(GENERATOR_IS_UMAKE TRUE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
 set(COMPILER_IS_MSVC TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
 set(COMPILER_IS_GNU TRUE)
else()
message(FATAL_ERROR "Unsupported compiler")
endif()

if(${GENERATOR_IS_UMAKE})
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
 ${NXBX_ROOT_DIR}/src/common
 ${NXBX_ROOT_DIR}/src/nxbx
 ${NXBX_ROOT_DIR}/src/qt
 ${NXBX_ROOT_DIR}/src/nxbx/fs
 ${NXBX_ROOT_DIR}/src/nxbx/hw
)

set(HEADERS
 "${NXBX_ROOT_DIR}/src/common/clock.hpp"
 "${NXBX_ROOT_DIR}/src/common/files.hpp"
 "${NXBX_ROOT_DIR}/src/common/isettings.hpp"
 "${NXBX_ROOT_DIR}/src/common/logger.hpp"
 "${NXBX_ROOT_DIR}/src/common/host.hpp"
 "${NXBX_ROOT_DIR}/src/common/settings.hpp"
 "${NXBX_ROOT_DIR}/src/common/util.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/console.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/io.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/kernel.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/kernel_head_ref.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/paths.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/pe.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/urls.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/xbe.hpp"
 "${NXBX_ROOT_DIR}/src/qt/main_window.hpp"
 "${NXBX_ROOT_DIR}/src/qt/qthost.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/fs/fatx.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/fs/xdvdfs.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/adm1032.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/cmos.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/cpu.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/eeprom.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/machine.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/pci.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/pic.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/pit.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/smbus.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/smc.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/usb/ohci.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/usb/ohci_reg_defs.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/conexant.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/vga.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/nv2a.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/nv2a_defs.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pbus.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pcrtc.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pfb.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pfifo.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pgraph.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pmc.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pramdac.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pramin.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/ptimer.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pvga.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pvideo.hpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/puser.hpp"
)

set(SOURCES
 "${NXBX_ROOT_DIR}/src/common/clock.cpp"
 "${NXBX_ROOT_DIR}/src/common/files.cpp"
 "${NXBX_ROOT_DIR}/src/common/host.cpp"
 "${NXBX_ROOT_DIR}/src/common/settings.cpp"
 "${NXBX_ROOT_DIR}/src/common/util.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/console.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/io.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/kernel.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/paths.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/xbe.cpp"
 "${NXBX_ROOT_DIR}/src/qt/main.cpp"
 "${NXBX_ROOT_DIR}/src/qt/main_window.cpp"
 "${NXBX_ROOT_DIR}/src/qt/main_window.ui"
 "${NXBX_ROOT_DIR}/src/qt/qthost.cpp"
 "${NXBX_ROOT_DIR}/src/qt/themes.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/fs/fatx.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/fs/xdvdfs.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/adm1032.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/cmos.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/cpu.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/eeprom.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/pci.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/pic.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/pit.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/smbus.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/smc.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/usb/ohci.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/conexant.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/vga.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/nv2a.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pbus.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pcrtc.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pfb.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pfifo.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pgraph.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pmc.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pramdac.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pramin.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/ptimer.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pvga.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/pvideo.cpp"
 "${NXBX_ROOT_DIR}/src/nxbx/hw/video/gpu/puser.cpp"
)

source_group(TREE ${NXBX_ROOT_DIR} PREFIX header FILES ${HEADERS})
source_group(TREE ${NXBX_ROOT_DIR} PREFIX source FILES ${SOURCES})

add_executable(nxbx ${HEADERS} ${SOURCES})
target_link_libraries(nxbx
	PRIVATE cpu
	PRIVATE Qt6::Core
	PRIVATE Qt6::Gui
	PRIVATE Qt6::Widgets
)

target_compile_definitions(nxbx PRIVATE QT_UI_BUILD QT_NO_EXCEPTIONS)
if(${COMPILER_IS_MSVC})
 set(CMAKE_CXX_FLAGS "/EHsc /Zc:preprocessor")
 target_compile_definitions(nxbx PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
endif()

if(${GENERATOR_IS_VS})
set_property(DIRECTORY "${NXBX_ROOT_DIR}" PROPERTY VS_STARTUP_PROJECT nxbx)
endif()

if(${GENERATOR_IS_VS})
add_custom_command(
	TARGET nxbx POST_BUILD
	COMMAND cd $<TARGET_FILE_DIR:nxbx> &&
	"${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:nxbx>" --dir . --libdir . --plugindir plugins --translationdir translations --force
)
elseif(${GENERATOR_IS_UMAKE})
add_custom_command(
	TARGET nxbx POST_BUILD
	COMMAND wget -P ${CMAKE_CURRENT_LIST_DIR}/build "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
	COMMAND wget -P ${CMAKE_CURRENT_LIST_DIR}/build "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
	COMMAND chmod +x ${CMAKE_CURRENT_LIST_DIR}/build/linuxdeploy*.AppImage
	COMMAND ${CMAKE_CURRENT_LIST_DIR}/build/linuxdeploy-x86_64.AppImage --appdir nxbx -e ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nxbx -i ${CMAKE_CURRENT_LIST_DIR}/resources/images/nxbx.png -d ${CMAKE_CURRENT_LIST_DIR}/resources/nxbx.desktop --plugin qt --output appimage
)
endif()
