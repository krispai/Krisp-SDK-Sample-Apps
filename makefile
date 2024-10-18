.PHONY: build
build: clean
	mkdir build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB}
	${MAKE} -C build VERBOSE=1

.PHONY: al
	mkdir build
	cmake -B build -S cmake \
		-D KRISP_SDK_PATH=${KRISP_SDK_PATH} \
		-D LIBSNDFILE_INC=${LIBSNDFILE_INC} \
		-D LIBSNDFILE_LIB=${LIBSNDFILE_LIB} \
		-D AL=1
	${MAKE} -C build VERBOSE=1

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
