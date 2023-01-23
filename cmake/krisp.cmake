if(NOT DEFINED ENV{KRISP_INC})
	message(FATAL_ERROR "KRISP_INC is not defined")
endif()

if(NOT DEFINED ENV{KRISP_LIB})
	message(FATAL_ERROR "KRISP_LIB is not defined")
endif()

set(KRISP_INC $ENV{KRISP_INC})
set(KRISP_LIB $ENV{KRISP_LIB})
