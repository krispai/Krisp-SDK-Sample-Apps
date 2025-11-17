if (APPLE)
	link_libraries("-framework CoreFoundation" "-framework Foundation")
endif()

# Use the SDK paths (set in CMakeLists.txt based on KRISP_COPY_SDK option)
find_library(LIBKRISP_ABSPATH NAMES krisp-audio-sdk PATHS ${KRISP_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBKRISP_ABSPATH)
	message(FATAL_ERROR "Can't find krisp-audio-sdk in ${KRISP_LIB_DIR} (USE_KRISP_DYNAMIC_LIBRARY=${USE_KRISP_DYNAMIC_LIBRARY})")
endif()
message(STATUS "Using Krisp SDK library: ${LIBKRISP_ABSPATH}")

if (WIN32)
	include(krisp.third.party.win.cmake)
elseif (APPLE)
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*(arm|ARM).*")
		include(krisp.third.party.mac.arm.cmake)
	else ()
		include(krisp.third.party.mac.x64.cmake)
	endif()
elseif (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES ".*(aarch64).*")
		include(krisp.third.party.linux.aarch64.cmake)
	else ()
		include(krisp.third.party.linux.x64.cmake)
	endif()
else ()
	MESSAGE(FATAL_ERROR "NOT IMPLEMENTED YET")
endif()

set(KRISP_LIBS
	${LIBKRISP_ABSPATH}
	${KRISP_THIRDPARTY_LIBS}
)
	
