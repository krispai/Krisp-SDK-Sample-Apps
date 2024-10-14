BUILD_PYTHON = 0
BUILD_DLL = 0
BUILD_NODEJS = 0

.PHONY: build
build:
	mkdir -p build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB} \
		-D BUILD_PYTHON_SAMPLE=${BUILD_PYTHON} \
		-D BUILD_SHARED_LIBRARY=${BUILD_DLL} \
		-D BUILD_NODEJS_SAMPLE=${BUILD_NODEJS} \
		-D NODE_INCLUDE_DIR=${NODE_INC}
	${MAKE} -C build VERBOSE=1

.PHONY: python
python:
	${MAKE} BUILD_PYTHON=1 build
	cp src/sample-python/process_wav.py bin/

.PHONY: node
node:
	${MAKE} BUILD_NODEJS=1 build

.PHONY: dll
dll:
	${MAKE} BUILD_DLL=1 build

.PHONY: vs
vs:
	mkdir vsbuild
	cmake -B vsbuild -S cmake

.PHONY: run
run:
	cd test && ./nc-sample-test-driver.sh

.PHONY: clean
clean:
	rm -rf build
	rm -rf bin
	rm -rf node_modules
	rm -f package.json
	rm -rf ./src/sample-node/node_modules/
