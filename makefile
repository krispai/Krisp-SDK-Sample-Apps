BUILD_PYTHON = 0

.PHONY: build
build:
	mkdir -p build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB} \
		-D BUILD_PYTHON_SAMPLE=${BUILD_PYTHON}
	${MAKE} -C build VERBOSE=1

.PHONY: python
python:
	${MAKE} BUILD_PYTHON=1 build
	cp src/sample-python/process_wav.py bin/

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
