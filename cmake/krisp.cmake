set(KRISP_LIB_DIR ${KRISP_SDK_PATH}/lib)
find_library(LIBKRISP_ABSPATH NAMES krisp-audio-sdk PATHS ${KRISP_LIB_DIR})
if (NOT LIBKRISP_ABSPATH) 
	message(FATAL_ERROR "Can't find krisp-audio-sdk in ${KRISP_LIB_DIR}")
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*(arm|ARM).*")
	set (ARM_CPU 1)
else ()
	set (ARM_CPU 0)
endif()

set(KRISP_3PARTY_LIB_DIR ${KRISP_SDK_PATH}/external)
if (WIN32)
	include(krisp.third.party.win.x64.cmake)
elseif (APPLE AND ARM_CPU)
	include(krisp.third.party.mac.arm.cmake)
elseif (APPLE)
	include(krisp.third.party.mac.x64.cmake)
elseif (UNIX AND NOT APPLE AND NOT ARM_CPU)
	include(krisp.third.party.linux.x64.cmake)
elseif (UNIX AND NOT APPLE AND ARM_CPU)
	MESSAGE(FATAL_ERROR "NOT IMPLEMENTED YET")
endif()

set(KRISP_LIBS 
	${LIBKRISP_ABSPATH}
	${KRISP_THIRDPARTY_LIBS}
)
