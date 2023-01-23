.PHONY: build
build:
	mkdir build
	cmake -B build -S cmake
	${MAKE} -C build VERBOSE=1

.PHONY: clean
clean:
	rm -rf build
	rm -rf bin
