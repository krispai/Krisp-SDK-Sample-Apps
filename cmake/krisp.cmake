find_library(LIBKRISP_ABSPATH NAMES krisp-audio-sdk PATHS ${KRISP_LIB_DIR})
if (NOT LIBKRISP_ABSPATH) 
	message(FATAL_ERROR "Can't find krisp-audio-sdk in ${KRISP_LIB_DIR}")
endif()

if (WIN32)
	include(krisp.third.party.win.x64.cmake)
elseif (APPLE AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*(arm|ARM).*")
	include(krisp.third.party.mac.arm.cmake)
elseif (APPLE)
	include(krisp.third.party.mac.x64.cmake)
elseif (UNIX AND NOT APPLE)
	message(FATAL_ERROR "not implemented yet")
endif()

set(KRISP_LIBS 
	${LIBKRISP_ABSPATH}
	${KRISP_THIRDPARTY_LIBS}
)
