.PHONY: build
build: clean
	mkdir build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB}
	${MAKE} -C build VERBOSE=1

.PHONY: stt
stt: clean
	mkdir build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB} \
		-D STT=1
	${MAKE} -C build VERBOSE=1

.PHONY: vs
vs:
	mkdir vsbuild
	cmake -B vsbuild -S cmake

.PHONY: run
run:
	cd test && ./nc-sample-test-driver.sh

.PHONY: clean
clean:
	if [ -d "./build" ]; then \
		rm -rf build; \
	fi
	if [ -d "./bin" ]; then \
		rm -rf bin; \
	fi
