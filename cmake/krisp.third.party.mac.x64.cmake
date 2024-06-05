if(NOT DEFINED KRISP_3PARTY_LIB_DIR)
	message(FATAL_ERROR "KRISP_3PARTY_LIB_DIR is not defined")
endif()

find_library(LIBRESAMPLE_ABSPATH NAMES resample PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBRESAMPLE_ABSPATH) 
	message(FATAL_ERROR "Can't find libresample static library in the ${KRISP_3PARTY_LIB_DIR}")
endif()

find_library(LIBCRYPTO_ABSPATH NAMES crypto PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBCRYPTO_ABSPATH) 
	message(FATAL_ERROR "Can't find libcrypto static library in the ${KRISP_3PARTY_LIB_DIR}")
endif()

find_library(LIBONNXRUNTIME_ABSPATH NAMES onnxruntime PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBONNXRUNTIME_ABSPATH) 
	message(FATAL_ERROR "Can't find libonnxruntime static library in the ${KRISP_3PARTY_LIB_DIR}")
endif()

set(LPREFIX ${CMAKE_STATIC_LIBRARY_PREFIX})
set(LSUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})

find_library(mkl_seq NAME ${LPREFIX}mkl_sequential${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if (NOT mkl_seq)
	message(FATAL_ERROR ${LPREFIX}mkl_sequential${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif()

find_library(mkl_core NAME ${LPREFIX}mkl_core${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if (NOT mkl_core)
	message(FATAL_ERROR ${LPREFIX}mkl_core${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif()

find_library(mkl_lp64 NAME ${LPREFIX}mkl_intel_lp64${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if(NOT mkl_lp64)
	message(FATAL_ERROR ${LPREFIX}mkl_intel_lp64${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif()

set(MKL_LIB_LIST ${mkl_lp64} ${mkl_seq} ${mkl_core})

set(KRISP_THIRDPARTY_LIBS 
	${MKL_LIB_LIST}
	${LIBCRYPTO_ABSPATH}
	${LIBRESAMPLE_ABSPATH}
	${LIBONNXRUNTIME_ABSPATH}
)

if (DEFINED STT)
	include(krisp.third.party.mac.x64.stt.cmake)
	set(KRISP_THIRDPARTY_LIBS ${KRISP_THIRDPARTY_LIBS} ${STT_LIBS})
endif()
