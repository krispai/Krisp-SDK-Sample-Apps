.PHONY: build
build:
	mkdir build
	cmake -B build -S cmake
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
	rm -rf build
	rm -rf bin
