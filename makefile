.PHONY: nc
nc:
	mkdir -p build
	cmake -B build -S cmake \
		-D BUILD_SAMPLE_NC=1 \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB}
	${MAKE} -C build VERBOSE=1

.PHONY: dll
dll:
	mkdir -p build
	cmake -B build -S cmake \
		-D BUILD_SHARED_LIBRARY=1 \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH}
	${MAKE} -C build VERBOSE=1

.PHONY: vs
vs:
	mkdir vsbuild
	cmake -B vsbuild -S cmake


.PHONY: python
python:
	mkdir -p build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D BUILD_PYTHON_SAMPLE=1
	${MAKE} -C build VERBOSE=1
	cp src/sample-python/process_wav.py bin/

.PHONY: node
node:
	mkdir -p build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D BUILD_NODEJS_SAMPLE=1 \
		-D NODE_INCLUDE_DIR=${NODE_INC}
	${MAKE} -C build VERBOSE=1

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
