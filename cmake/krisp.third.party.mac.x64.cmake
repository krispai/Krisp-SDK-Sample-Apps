set(LIBRESAMPLE_LIB_DIR ${KRISP_SDK_PATH}/ext_deps/resample/lib)
find_library(LIBRESAMPLE_ABSPATH NAMES resample PATHS ${LIBRESAMPLE_LIB_DIR} NO_DEFAULT_PATH)
if (NOT LIBRESAMPLE_ABSPATH) 
	message(FATAL_ERROR "Can't find libresample static library in ${LIBRESAMPLE_LIB_DIR}")
endif()

set(MKL_LIB_DIR ${KRISP_SDK_PATH}/ext_deps/mkl/lib)
include(mkl.cmake)
# this will output MKL_LIB_LIST

set(KRISP_THIRDPARTY_LIBS 
	${LIBRESAMPLE_ABSPATH}
	${MKL_LIB_LIST}
)
