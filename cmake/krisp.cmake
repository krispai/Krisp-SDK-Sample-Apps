find_library(LIBKRISP_ABSPATH NAMES krisp-audio-sdk PATHS ${KRISP_LIB_DIR})
if (NOT LIBKRISP_ABSPATH) 
	message(FATAL_ERROR "Can't find krisp-audio-sdk")
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*(arm|ARM).*")
	include(krisp.third.party.mac.arm.cmake)
else()
	include(krisp.third.party.mac.x64.cmake)
endif()

set(KRISP_LIBS 
	${LIBKRISP_ABSPATH}
	${KRISP_THIRDPARTY_LIBS}
)
