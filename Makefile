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

BUILD_DIR ?= build
CHROMEOVER ?= false
CMAKE ?= cmake
METRICS_API_KEY ?= ""
RELEASE ?= false
TREAT_WARNINGS_AS_ERRORS ?= false

# Release mode vs normal debug mode
ifeq (${RELEASE}, true)
	CMAKE_BUILD_TYPE ?= RelWithDebInfo
	PACKAGE_FLAGS ?= --release
	WIN32_CONSOLE ?= OFF
else
	CMAKE_BUILD_TYPE ?= Debug
	WIN32_CONSOLE ?= ON
	PACKAGE_FLAGS ?=
endif


# Default target: build gondar and run the tests
all: build-gondar test


# Build gondar executable
build-gondar: print-config update-submodules
	mkdir -p "${BUILD_DIR}" && \
	cd ${BUILD_DIR} && \
		${CMAKE} .. \
			-DCHROMEOVER=${CHROMEOVER} \
			-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
			-DTREAT_WARNINGS_AS_ERRORS=${TREAT_WARNINGS_AS_ERRORS} \
			-DWIN32_CONSOLE=${WIN32_CONSOLE} \
			-DMETRICS_API_KEY:STRING=${METRICS_API_KEY} \
			-DRELEASE=${RELEASE} && \
		make -j


clean:
	rm -rf ${BUILD_DIR}


# Start an interactive shell in the MXE container. The current working
# directory in the host is mounted in the container under /opt/host.
docker-mxe-shell:
	sudo docker run \
		--volume ${PWD}:/opt/host \
		--interactive \
		--tty \
		--workdir /opt/host \
		neverware/gondar-build-mxe:v3 \
		bash


format:
	CLANG_FORMAT=${CLANG_FORMAT} infra/format.py


help:
	@echo "usage: make [build-gondar|clean|docker-mxe-shell|format|jenkins|test]"


# The Jenkins entry point. The Jenkins job should be configured to run
# "make jenkins".
jenkins: jenkins-linux jenkins-win32


jenkins-linux:
	sudo docker build -f docker/gondar-linux.Dockerfile .


jenkins-win32:
	python package.py ${PACKAGE_FLAGS}


print-config:
	@echo "Build config {"
	@echo "  BUILD_DIR: ${BUILD_DIR}"
	@echo "  CHROMEOVER: ${CHROMEOVER}"
	@echo "  CMAKE: ${CMAKE}"
	@echo "  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}"
	@echo "  METRICS_API_KEY: ${METRICS_API_KEY}"
	@echo "  PACKAGE_FLAGS: '${PACKAGE_FLAGS}' (only affects docker win32 builds)"
	@echo "  RELEASE: ${RELEASE}"
	@echo "  TREAT_WARNINGS_AS_ERRORS: ${TREAT_WARNINGS_AS_ERRORS}"
	@echo "  WIN32_CONSOLE: ${WIN32_CONSOLE} (only affects win32 builds)"
	@echo "}"


# Run tests in headless mode
test: build-gondar
	${BUILD_DIR}/tests -platform offscreen

# Run tests which take quite some time
bigtest: build-gondar
	${BUILD_DIR}/bigtests -platform offscreen


update-submodules:
# Skip submodule update if not in a git workspace
ifeq ($(realpath .git),)
	@echo "skipping submodule update"
else
	git submodule update --init
endif


# The targets under "build-" are phony targets because those targets
# handle the real dependency tracking internally
.PHONY: all \
		build-gondar \
		clean \
		docker-mxe-shell \
		format \
		jenkins \
		jenkins-linux \
		jenkins-win32 \
		print-config \
		test \
		update-submodules
