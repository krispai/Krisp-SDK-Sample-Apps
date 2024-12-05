BUILD_SAMPLE_NC := 0
BUILD_SAMPLE_AL := 0
BUILD_SAMPLE_PYTHON := 0
BUILD_SAMPLE_NODEJS := 0

ifneq ($(filter nc,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_NC := 1
endif

ifneq ($(filter al,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_AL := 1
endif

ifneq ($(filter python,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_PYTHON := 1
endif

ifneq ($(filter node,$(MAKECMDGOALS)),)
	BUILD_SAMPLE_NODEJS := 1
endif

.PHONY: nc al python build run clean node

nc: build

al: build

python: build

node: build

build:
	mkdir -p build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB} \
		-D NODE_INC=${NODE_INC} \
		-D BUILD_SAMPLE_NC=$(BUILD_SAMPLE_NC) \
		-D BUILD_SAMPLE_AL=$(BUILD_SAMPLE_AL) \
		-D BUILD_SAMPLE_PYTHON=$(BUILD_SAMPLE_PYTHON) \
		-D BUILD_SAMPLE_NODEJS=$(BUILD_SAMPLE_NODEJS) \
		-D PYTHON3_PATH=$(PYTHON3_PATH)

	${MAKE} -C build VERBOSE=1
	@if [ "$(BUILD_SAMPLE_PYTHON)" -eq "1" ]; then \
		cp ./src/sample-python/process_wav.py bin/; \
	fi

run:
	cd test && ./nc-sample-test-driver.sh

clean:
	if [ -d "./build" ]; then \
		rm -rf build; \
	fi
	if [ -d "./bin" ]; then \
		rm -rf bin; \
	fi
