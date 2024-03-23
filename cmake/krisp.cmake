if(NOT DEFINED ENV{KRISP_INC})
	message(FATAL_ERROR "KRISP_INC is not defined")
endif()

if(NOT DEFINED ENV{KRISP_LIB})
	message(FATAL_ERROR "KRISP_LIB is not defined")
endif()

include(krisp.third.party.cmake)

set(KRISP_LIBS 
	${LIBKRISP_ABSPATH}
	${KRISP_THIRDPARTY_LIBS}
)
