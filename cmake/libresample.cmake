if(NOT DEFINED ENV{LIBRESAMPLE_LIB})
	message(FATAL_ERROR "LIBRESAMPLE_LIB is not defined")
endif()

find_library(LIBRESAMPLE_ABSPATH NAMES resample PATHS $ENV{LIBRESAMPLE_LIB})
if (NOT LIBRESAMPLE_ABSPATH) 
	message(FATAL_ERROR "Can't find libresample static library")
endif()
#link_libraries(${LIBRESAMPLE_ABSPATH})
