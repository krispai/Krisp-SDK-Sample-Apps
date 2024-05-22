if(NOT DEFINED LIBSNDFILE_INC)
	message(FATAL_ERROR "LIBSNDFILE_INC is not defined")
endif()

if(NOT DEFINED LIBSNDFILE_LIB)
	message(FATAL_ERROR "LIBSNDFILE_LIB is not defined")
endif()

find_library(LIBSNDFILE_ABSPATH NAMES libsndfile-1 sndfile PATHS ${LIBSNDFILE_LIB})
if (NOT LIBSNDFILE_ABSPATH)
	message(FATAL_ERROR " libsndfile-1, libsndfile library is missing in ${LIBSNDFILE_LIB}")
endif()
