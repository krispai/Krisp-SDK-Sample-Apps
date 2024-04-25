if(NOT DEFINED KRISP_3PARTY_LIB_DIR)
	message(FATAL_ERROR "KRISP_3PARTY_LIB_DIR is not defined")
endif()

find_library(LIBRESAMPLE_ABSPATH NAMES resample PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBRESAMPLE_ABSPATH) 
	message(FATAL_ERROR "Can't find resample static library")
endif()

find_library(LIBZIP_ABSPATH NAMES zip PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBZIP_ABSPATH) 
	message(FATAL_ERROR "Can't find zip static library")
endif()

find_library(LIBZ_ABSPATH NAMES zlib PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBZ_ABSPATH) 
	message(FATAL_ERROR "Can't find zlib static library")
endif()

find_library(LIBZSTD_ABSPATH NAMES zstd_static PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBZSTD_ABSPATH) 
	message(FATAL_ERROR "Can't find zstd_static static library")
endif()

find_library(LIBBZ2_ABSPATH NAMES bz2 PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBBZ2_ABSPATH) 
	message(FATAL_ERROR "Can't find bz2 static library")
endif()

find_library(LIBLZMA_ABSPATH NAMES lzma PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBLZMA_ABSPATH) 
	message(FATAL_ERROR "Can't find lzma static library")
endif()

find_library(LIBONNXRUNTIME_ABSPATH NAMES onnxruntime PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBONNXRUNTIME_ABSPATH) 
	message(FATAL_ERROR "Can't find onnxruntime static library")
endif()

find_library(LIBCTC_DECODER_ABSPATH NAMES ctc-decoder PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBCTC_DECODER_ABSPATH) 
	message(FATAL_ERROR "Can't find ctc-decoder static library")
endif()

find_library(LIBSENTENCEPIECE_ABSPATH NAMES sentencepiece PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBSENTENCEPIECE_ABSPATH) 
	message(FATAL_ERROR "Can't find sentencepiece static library")
endif()

find_library(LIBCPPNINI_ABSPATH NAMES cppnini PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBCPPNINI_ABSPATH) 
	message(FATAL_ERROR "Can't find cppnini static library")
endif()

find_library(LIBOPENFST_USEFUL_ABSPATH NAMES openfst-useful PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBOPENFST_USEFUL_ABSPATH) 
	message(FATAL_ERROR "Can't find openfst-useful static library")
endif()

find_library(LIBPYNINI_USEFUL NAMES pynini-useful PATHS ${KRISP_3PARTY_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBPYNINI_USEFUL) 
	message(FATAL_ERROR "Can't find pynini-useful static library")
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

find_library(mkl_lp64 NAME ${LPREFIX}mkl_intel_ilp64${LSUFFIX} PATHS ${KRISP_3PARTY_LIB_DIR})
if(NOT mkl_lp64)
	message(FATAL_ERROR ${LPREFIX}mkl_intel_ilp64${LSUFFIX} " is missing in the ${KRISP_3PARTY_LIB_DIR}")
endif()

set(MKL_LIB_LIST ${mkl_lp64} ${mkl_seq} ${mkl_core})

set(KRISP_THIRDPARTY_LIBS 
	${MKL_LIB_LIST}
	${LIBRESAMPLE_ABSPATH}
	${LIBZIP_ABSPATH}
	${LIBZ_ABSPATH}
	${LIBZSTD_ABSPATH}
	${LIBBZ2_ABSPATH}
	${LIBLZMA_ABSPATH}
	${LIBONNXRUNTIME_ABSPATH}
	${LIBCTC_DECODER_ABSPATH}
	${LIBSENTENCEPIECE_ABSPATH}
	${LIBCPPNINI_ABSPATH}
	${LIBOPENFST_USEFUL_ABSPATH}
	${LIBPYNINI_USEFUL}
)
