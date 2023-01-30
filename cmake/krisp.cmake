if(NOT DEFINED ENV{KRISP_INC})
	message(FATAL_ERROR "KRISP_INC is not defined")
endif()

if(NOT DEFINED ENV{KRISP_LIB})
	message(FATAL_ERROR "KRISP_LIB is not defined")
endif()

find_library(LIBKRISP_ABSPATH NAMES krisp-audio-sdk-static PATHS $ENV{KRISP_LIB})
if (NOT LIBKRISP_ABSPATH) 
	message(FATAL_ERROR "Can't find krisp-audio-sdk")
endif()
include_directories($ENV{KRISP_INC})
link_libraries(${LIBKRISP_ABSPATH})
