cmake_minimum_required(VERSION 3.5)
project("Krisp SDK Sample Apps")

# Set 'Release' or 'Debug'
set(CMAKE_BUILD_TYPE Debug)

get_filename_component(ROOT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ROOT_DIR}/bin)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# is needed to get ${LIBSNDFILE_ABSPATH}
include(libsndfile.cmake)

# Krisp SDK libraries are applied to all targets
include(krisp.cmake)

# Blas libraries are appleid to all targets
include(blas.cmake)

set(APPNAME_NC sample-nc)
#set(APPNAME_VAD sample-vad)
#set(APPNAME_NOISE_DB sample-noise-db)

add_executable(${APPNAME_NC} 
    ${ROOT_DIR}/src/sample-nc/main.cpp
    ${ROOT_DIR}/src/utils/wave_reader.cpp
    ${ROOT_DIR}/src/utils/wave_writer.cpp
    ${ROOT_DIR}/src/utils/argument_parser.cpp
)

target_include_directories(
	${APPNAME_NC}
	PRIVATE
    ${ROOT_DIR}/src/utils
	${LIBSNDFILE_INC}
)

target_link_libraries(${APPNAME_NC} ${LIBSNDFILE_ABSPATH})
