BUILD_SAMPLE_NC := 0
BUILD_SAMPLE_AL := 0
BUILD_SAMPLE_PYTHON := 0

ifneq ($(filter nc,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_NC := 1
endif

ifneq ($(filter al,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_AL := 1
endif

ifneq ($(filter python,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_PYTHON := 1
endif

.PHONY: nc al python build run clean

nc: build

al: build

python: build

build:
	mkdir -p build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB} \
		-D BUILD_SAMPLE_NC=$(BUILD_SAMPLE_NC) \
		-D BUILD_SAMPLE_AL=$(BUILD_SAMPLE_AL) \
		-D BUILD_PYTHON_SAMPLE=$(BUILD_SAMPLE_PYTHON) 
	${MAKE} -C build VERBOSE=1
	@if [ "$(BUILD_SAMPLE_PYTHON)" -eq "1" ]; then \
		cp src/sample-python/process_wav.py bin/; \
	fi

run:
	cd test && ./nc-sample-test-driver.sh

runtime:
	cp ${KRISP_SDK_PATH}/

clean:
	if [ -d "./build" ]; then \
		rm -rf build; \
	fi
	if [ -d "./bin" ]; then \
		rm -rf bin; \
	fi
