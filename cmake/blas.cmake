if (${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*(arm|ARM).*")
	message(STATUS "ARM CPU is used")
	link_libraries(blas)
else()
	message(STATUS "x86/amd64 CPU is used, checking Intel MKL libraries")

	if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
		message(FATAL_ERROR "CMake is not configured for x86 32 bit platform")
	endif()

	include_directories(${MKL_INC})
	add_compile_options(-DMKL_ILP64)
	add_compile_options(-m64)

	link_directories(${MKL_LIB})
	link_libraries(mkl_intel_ilp64)
	link_libraries(mkl_sequential)
	link_libraries(mkl_core)
endif()
