if(NOT DEFINED ENV{KRISP_INC})
	message(FATAL_ERROR "KRISP_INC is not defined")
endif()

if(NOT DEFINED ENV{KRISP_LIB})
	message(FATAL_ERROR "KRISP_LIB is not defined")
endif()

find_library(LIBKRISP_ABSPATH NAMES krisp-audio-sdk PATHS $ENV{KRISP_LIB})
if (NOT LIBKRISP_ABSPATH) 
	message(FATAL_ERROR "Can't find krisp-audio-sdk")
endif()

#find_library(LIBKRISPLEGACY_ABSPATH NAMES krisp-legacy-sdk PATHS $ENV{KRISP_LIB})
#if (NOT LIBKRISP_ABSPATH) 
#	message(FATAL_ERROR "Can't find libkrisp-legacy-sdk")
#endif()

# TODO: rename LIBRESAMPLE_LIB to "EXTERNAL" or something like that

if(NOT DEFINED ENV{LIBRESAMPLE_LIB})
	message(FATAL_ERROR "LIBRESAMPLE_LIB is not defined")
endif()

find_library(LIBRESAMPLE_ABSPATH NAMES resample PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBRESAMPLE_ABSPATH) 
	message(FATAL_ERROR "Can't find libresample static library")
endif()

find_library(LIBXNNPACK_ABSPATH NAMES XNNPACK PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBXNNPACK_ABSPATH) 
	message(FATAL_ERROR "Can't find libXNNPACK static library")
endif()

find_library(LIBZIP_ABSPATH NAMES zip PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBZIP_ABSPATH) 
	message(FATAL_ERROR "Can't find libzip static library")
endif()

find_library(LIBZ_ABSPATH NAMES z PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBZ_ABSPATH) 
	message(FATAL_ERROR "Can't find libz static library")
endif()

find_library(LIBZSTD_ABSPATH NAMES zstd PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBZSTD_ABSPATH) 
	message(FATAL_ERROR "Can't find libzsd static library")
endif()

find_library(LIBBZ2_ABSPATH NAMES bz2 PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBBZ2_ABSPATH) 
	message(FATAL_ERROR "Can't find libbz2 static library")
endif()

find_library(LIBCPUINFO_ABSPATH NAMES cpuinfo PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBCPUINFO_ABSPATH) 
	message(FATAL_ERROR "Can't find libcpuinfo static library")
endif()

find_library(LIBLZMA_ABSPATH NAMES lzma PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBLZMA_ABSPATH) 
	message(FATAL_ERROR "Can't find liblzma static library")
endif()

find_library(LIBPTHREADPOOL_ABSPATH NAMES pthreadpool PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBPTHREADPOOL_ABSPATH) 
	message(FATAL_ERROR "Can't find pthreadpool static library")
endif()

find_library(LIBFFTW3F_ABSPATH NAMES fftw3f PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBFFTW3F_ABSPATH) 
	message(FATAL_ERROR "Can't find libfftw3f static library")
endif()

find_library(LIBFFTW3F_THREADS_ABSPATH NAMES fftw3f_threads PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBFFTW3F_THREADS_ABSPATH) 
	message(FATAL_ERROR "Can't find libfftw3f_threads static library")
endif()

find_library(LIBONNXRUNTIME_ABSPATH NAMES onnxruntime PATHS $ENV{LIBRESAMPLE_LIB} NO_DEFAULT_PATH)
if (NOT LIBONNXRUNTIME_ABSPATH) 
	message(FATAL_ERROR "Can't find libonnxruntime static library")
endif()

set(KRISP_LIBS 
	${LIBKRISP_ABSPATH}
	${LIBRESAMPLE_ABSPATH}
	${LIBXNNPACK_ABSPATH}
	${LIBZIP_ABSPATH}
	${LIBZ_ABSPATH}
	${LIBZSTD_ABSPATH}
	${LIBBZ2_ABSPATH}
	${LIBCPUINFO_ABSPATH}
	${LIBLZMA_ABSPATH}
	${LIBPTHREADPOOL_ABSPATH}
	${LIBFFTW3F_ABSPATH}
	${LIBFFTW3F_THREADS_ABSPATH}
	${LIBONNXRUNTIME_ABSPATH}
	/Users/atatalyan/dev/krisp-sdk/krisp-audio-sdk-8.0.0/external/libctc-decoder.a
	/Users/atatalyan/dev/krisp-sdk/krisp-audio-sdk-8.0.0/external/libsentencepiece.a
	/Users/atatalyan/dev/krisp-sdk/krisp-audio-sdk-8.0.0/external/libcppnini.a
	/Users/atatalyan/dev/krisp-sdk/krisp-audio-sdk-8.0.0/external/libopenfst-useful.a
	/Users/atatalyan/dev/krisp-sdk/krisp-audio-sdk-8.0.0/external/libpynini-useful.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-nc-processor/lib/libkrisp-nc-processor.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-noise-db-processor/lib/libkrisp-noise-db-processor.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-asr-processor/lib/libkrisp-asr-processor.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-vad-processor/lib/libkrisp-vad-processor.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-legacy-sdk/lib/libkrisp-legacy-sdk.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-core/lib/libkrisp-core.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-media-sdk/lib/libkrisp-media-sdk.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-common/lib/libkrisp-common.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-common-manipulators/lib/libkrisp-common-manipulators.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-blas/lib/libkrisp-blas.a
	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-inference-engine/lib/libkrisp-inference-engine.a
	#/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-dsp/lib/libkrisp-dsp.a
	#	/Users/atatalyan/dev/gitrepos/krisp-media-sdk/build/krisp-unified-format/lib/libkrisp-unified-format.a
)
