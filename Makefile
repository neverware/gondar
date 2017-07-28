BUILD_DIR ?= build
CHROMEOVER ?= false
CMAKE ?= cmake
RELEASE ?= false
TREAT_WARNINGS_AS_ERRORS ?= false

# Some distros use different names for clang-format
ifneq (, $(shell which clang-format-4.0 2> /dev/null))
    CLANG_FORMAT ?= clang-format-4.0
else
    CLANG_FORMAT ?= clang-format
endif

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
			-DMETRICS_API_KEY:STRING=${METRICS_API_KEY} && \
		make -j


clean:
	rm -rf ${BUILD_DIR}


format:
	${CLANG_FORMAT} -i src/*.{h,cc} test/*.{h,cc}


help:
	@echo "usage: make [build-gondar|clean|format|jenkins|test]"


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
		format \
		jenkins \
		jenkins-linux \
		jenkins-win32 \
		print-config \
		test \
		update-submodules
