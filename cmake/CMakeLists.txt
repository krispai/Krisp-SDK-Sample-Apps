cmake_minimum_required(VERSION 3.5)
project("Krisp SDK Sample Apps")

# Set 'Release' or 'Debug'
set(CMAKE_BUILD_TYPE Debug)

get_filename_component(ROOT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ROOT_DIR}/bin)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MKL_INC ${ROOT_DIR}/deps/mkl/inc)
set(MKL_LIB ${ROOT_DIR}/deps/mkl/lib)

include(libsndfile.cmake)
include(krisp.cmake)

# TODO: check if useful for Linux
#include_directories(${OpenBLAS_INCLUDE_DIRS})
#link_directories(/opt/OpenBLAS/lib/)

# define dependency to Krisp Audio SDK static library
include_directories(${KRISP_INC})
link_directories(${KRISP_LIB})
link_libraries(krisp-audio-sdk-static)

include(blas.cmake)

set(SAMPLE_NC_SOURCES 
    ${ROOT_DIR}/src/sample-nc/main.cpp
    ${ROOT_DIR}/src/utils/wave_reader.cpp
    ${ROOT_DIR}/src/utils/wave_writer.cpp
    ${ROOT_DIR}/src/utils/argument_parser.cpp
)

set(APPNAME_NC sample-nc)
#set(APPNAME_VAD sample-vad)
#set(APPNAME_NOISE_DB sample-noise-db)

add_executable(${APPNAME_NC} ${SAMPLE_NC_SOURCES})

target_include_directories(
	${APPNAME_NC}
	PRIVATE
    ${ROOT_DIR}/src/utils
	${LIBSNDFILE_INC}
)

target_link_directories(
	${APPNAME_NC}
	PRIVATE
	${LIBSNDFILE_LIB}
)

target_link_libraries(${APPNAME_NC} sndfile)
