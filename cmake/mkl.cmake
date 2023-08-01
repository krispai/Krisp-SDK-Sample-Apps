if(NOT DEFINED ENV{MKL_LIB})
	message(FATAL_ERROR "MKL_LIB should be specified for this platform")
endif()

set(MKL_LIB $ENV{MKL_LIB})

set(LPREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
set(LSUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})

find_library(mkl_seq NAME ${LPREFIX}mkl_sequential${LSUFFIX} PATHS ${MKL_LIB})
if (NOT mkl_seq)
	message(FATAL_ERROR ${LPREFIX}mkl_sequential${LSUFFIX} " is missing")
endif()
find_library(mkl_core NAME ${LPREFIX}mkl_core${LSUFFIX} PATHS ${MKL_LIB})
if (NOT mkl_core)
	message(FATAL_ERROR ${LPREFIX}mkl_core${LSUFFIX} " is missing")
endif()

if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
	find_library(mkl_intel_c NAME ${LPREFIX}mkl_intel_c${LSUFFIX} PATHS ${MKL_LIB})
	if(NOT mkl_intel_c)
		message(FATAL_ERROR ${LPREFIX}mkl_intel_c${LSUFFIX} " is missing")
	endif()
	set(MKL_LIB_LIST ${mkl_intel_c} ${mkl_seq} ${mkl_core})
else ()
	find_library(mkl_lp64 NAME ${LPREFIX}mkl_intel_lp64${LSUFFIX} PATHS ${MKL_LIB})
	if(NOT mkl_lp64)
		message(FATAL_ERROR ${LPREFIX}mkl_intel_lp64${LSUFFIX} " is missing")
	endif()
	set(MKL_LIB_LIST ${mkl_lp64} ${mkl_seq} ${mkl_core})
endif ()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	include(mkl.linux.cmake)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	target_link_libraries(
		${APPNAME_NC}
		${MKL_LIB_LIST}
		pthread m dl
	)
	target_link_libraries(
		${DLLNAME}
		${MKL_LIB_LIST}
		pthread m dl
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_libraries(
		${APPNAME_NC}
		${MKL_LIB_LIST}
	)
	target_link_libraries(
		${DLLNAME}
		${MKL_LIB_LIST}
	)
endif()
