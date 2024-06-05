if(NOT DEFINED KRISP_3PARTY_LIB_DIR)
	message(FATAL_ERROR "KRISP_3PARTY_LIB_DIR is not defined")
endif()

find_library(LIBRESAMPLE_ABSPATH NAMES resample PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if(NOT LIBRESAMPLE_ABSPATH) 
	message(FATAL_ERROR "Can't find resample static library")
endif()

find_library(LIBCRYPTO_ABSPATH NAMES libcrypto PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if(NOT LIBCRYPTO_ABSPATH) 
	message(FATAL_ERROR "Can't find libcrypto static library")
endif()

find_library(LIBSSL_ABSPATH NAMES libssl PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if(NOT LIBSSL_ABSPATH) 
	message(FATAL_ERROR "Can't find libssl static library")
endif()

find_library(LIBONNXRUNTIME_ABSPATH NAMES onnxruntime PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if(NOT LIBONNXRUNTIME_ABSPATH) 
	message(FATAL_ERROR "Can't find onnxruntime static library")
endif()

set(LPREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
set(LSUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})

find_library(mkl_seq NAME ${LPREFIX}mkl_sequential${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if(NOT mkl_seq)
	message(FATAL_ERROR ${LPREFIX}mkl_sequential${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif ()

find_library(mkl_core NAME ${LPREFIX}mkl_core${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if(NOT mkl_core)
	message(FATAL_ERROR ${LPREFIX}mkl_core${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif ()

find_library(mkl_lp64 NAME ${LPREFIX}mkl_intel_ilp64${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if(NOT mkl_lp64)
	message(FATAL_ERROR ${LPREFIX}mkl_intel_ilp64${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif()

#find_library(CRYPT32 Crypt32)
#if (NOT CRYPT32)
#	message(FATAL_ERROR "Can't find the Crypt32.lib.")
#endif()
#find_library(WS2 Ws2_32)
#if(NOT WS2)
#	message(FATAL_ERROR "Can't find the Ws2_32.lib.")
#endif()

set(MKL_LIB_LIST ${mkl_lp64} ${mkl_seq} ${mkl_core})

set(KRISP_THIRDPARTY_LIBS 
	${MKL_LIB_LIST}
	${LIBSSL_ABSPATH}
	${LIBCRYPTO_ABSPATH}
	${LIBRESAMPLE_ABSPATH}
	${LIBONNXRUNTIME_ABSPATH}
	${LIBPYNINI_USEFUL}
	#${WS2}
	#${CRYPT32}
	Crypt32.lib
	Ws2_32.lib
)

if (DEFINED STT)
	include(krisp.third.party.win.x64.stt.cmake)
	set(KRISP_THIRDPARTY_LIBS ${KRISP_THIRDPARTY_LIBS} ${STT_LIBS})
endif()
