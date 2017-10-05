# Copyright 2017 Neverware
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.4)
project(gondar)

include(infra/gondar.cmake)

# Choose between free and paid product downloads
set(CHROMEOVER OFF CACHE BOOL "Build for Chromeover customers")

# Whether the build is release (console, grv.neverware.com, etc.)
set(RELEASE OFF CACHE BOOL "Release build")

# This is for the builders, to make sure new warnings don't creep in
set(TREAT_WARNINGS_AS_ERRORS OFF CACHE BOOL "Compile with -Werror")

# This setting is used in minizip's cmake file, setting it to off here
# because we don't need to handle encrypted zip files.
set(USE_AES OFF CACHE BOOL "Unused docstring")

# This is disabled in our release builds so that the ugly console
# doesn't show.
set(WIN32_CONSOLE ON CACHE BOOL "On Windows, show the console")

# Extra warnings sourced from:
# https://stackoverflow.com/questions/5088460
#
# TODO(nicholasbishop): add -Wsign-conversion,
# -Wold-style-cast, -Wcast-qual, -Wstrict-overflow=5
set(EXTRA_WARNINGS ${EXTRA_WARNINGS}
  # Warnings limited to C++
  $<$<COMPILE_LANGUAGE:CXX>:
    -Wctor-dtor-privacy
    -Wnoexcept
    -Woverloaded-virtual
    -Wsign-promo
    -Wstrict-null-sentinel
    -Wsuggest-override>

  -pedantic
  -Wall
  -Wextra

  # Warnings for both C and C++
  -Wcast-align
  -Wdisabled-optimization
  -Wformat=2
  -Winit-self
  -Wlogical-op
  -Wmissing-declarations
  -Wredundant-decls
  -Wshadow
  -Wundef
  -Wunused-macros
  -Wwrite-strings)

# C++14 needed for std::experimental::optional. (C++17 is not
# supported in the version of gcc used by MXE.)
set(CMAKE_CXX_STANDARD 14)

set(METRICS_API_KEY CACHE STRING "metrics API key")
configure_file(src/config.h.in src/config.h @ONLY)

if(${TREAT_WARNINGS_AS_ERRORS})
  set(EXTRA_WARNINGS ${EXTRA_WARNINGS} -Werror)
endif()

# This mingw define adds more modern string formatting than what
# msvcrt provides (C99/C11 vs C89). This makes extended format
# specifiers like "%lld" work.
add_definitions(-D__USE_MINGW_ANSI_STDIO=1)

# Build minizip
add_subdirectory(minizip)

# Required Qt components
find_package(Qt5 COMPONENTS Network Test Widgets REQUIRED)

# Static lib with convenience functions that make unzipping easier
add_library(minizip_extra STATIC minizip/minishared.c minizip/miniunz.c)
target_compile_definitions(minizip_extra PRIVATE _LARGEFILE64_SOURCE NOMAIN)
target_compile_options(minizip_extra PRIVATE
  ${EXTRA_WARNINGS}
  -Wno-format
  -Wno-missing-declarations
  -Wno-unused-macros)

# Static lib containing the bulk of gondar, shared between the
# application and test targets
add_library(app STATIC
  resources/gondarwizard.qrc
  src/about_dialog.cc
  src/admin_check_page.cc
  src/newest_image_url.cc
  src/chromeover_login_page.cc
  src/device.cc
  src/device_picker.cc
  src/device_select_page.cc
  src/diskwritethread.cc
  src/download_progress_page.cc
  src/downloader.cc
  src/error_page.cc
  src/gondarsite.cc
  src/gondarwizard.cc
  src/image_select_page.cc
  src/log.cc
  src/meepo.cc
  src/metric.cc
  src/neverware_unzipper.cc
  src/site_select_page.cc
  src/unzipthread.cc
  src/update_check.cc
  src/usb_insert_page.cc
  src/util.cc
  src/wizard_page.cc
  src/write_operation_page.cc)

set_target_properties(app PROPERTIES AUTOMOC ON AUTORCC ON)
target_compile_options(app PRIVATE ${EXTRA_WARNINGS})
target_include_directories(app SYSTEM PUBLIC minizip plog/include)
target_include_directories(app PRIVATE ${CMAKE_BINARY_DIR}/src)
target_link_libraries(app PUBLIC
  Qt5::Network Qt5::Widgets minizip minizip_extra)

# Gondar application
add_executable(cloudready-usb-maker src/main.cc)
target_link_libraries(cloudready-usb-maker app)

# Test application
add_executable(tests test/test.cc)
add_executable(bigtests test/bigtest.cc)
target_include_directories(tests PRIVATE .)
target_include_directories(bigtests PRIVATE .)
target_link_libraries(tests Qt5::Test app)
target_link_libraries(bigtests Qt5::Test app)
set_target_properties(tests PROPERTIES AUTOMOC ON)
set_target_properties(bigtests PROPERTIES AUTOMOC ON)

# Platform-specific build configuration
if(WIN32)
  include(infra/gdisk.cmake)
  if(NOT ${WIN32_CONSOLE})
    set_target_properties(cloudready-usb-maker PROPERTIES WIN32_EXECUTABLE ON)
  endif()

  fix_qt_static_link(app)

  target_link_libraries(app PRIVATE setupapi)
  target_link_libraries(app PRIVATE gdisk)
  target_sources(app PRIVATE src/gondar.cc)
  target_sources(cloudready-usb-maker PRIVATE resources/gondar.rc)
else()
  target_sources(app PRIVATE src/stubs.cc)
endif()
