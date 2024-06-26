cmake_minimum_required(VERSION 3.5)

if (WIN32)
	include(visualstudio.cmake NO_POLICY_SCOPE)
endif()

project("Krisp SDK Sample Apps")

# Set 'Release' or 'Debug'
set(CMAKE_BUILD_TYPE Debug)

get_filename_component(ROOT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ROOT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ROOT_DIR}/bin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX OR APPLE)
    add_compile_options(-Wall -Werror -pedantic)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(-Wextra -Wconversion -Wsign-conversion)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
			-Weverything
			-Wno-c++98-compat-pedantic
			-Wno-documentation-pedantic
			-Wdeprecated-declarations
			-Wno-deprecated-declarations
			-Wno-padded
		)
    endif()
endif()

if (NOT DEFINED KRISP_SDK_PATH)
	message(FATAL_ERROR "KRISP_SDK_PATH must be specified")
endif()

set(KRISP_INC_DIR ${KRISP_SDK_PATH}/include)                                            
set(KRISP_LIB_DIR ${KRISP_SDK_PATH}/lib)

# is needed to get ${LIBSNDFILE_ABSPATH} and ${LIBSNDFILE_INC}
include(libsndfile.cmake)

# Krisp SDK libraries are applied to all targets
include(krisp.cmake)

# Libresample is required for Krisp SDK
#include(libresample.cmake)

set(APPNAME_NC sample-nc)
set(DLLNAME krisp-dll)
set(APPNAME_DLL dll-test-app)
#set(APPNAME_VAD sample-vad)

add_compile_definitions(KRISP_AUDIO_STATIC)
if (WIN32 AND MSVC)
	add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

add_library(
	${DLLNAME} SHARED
	${ROOT_DIR}/src/sample-dll/dll-main.cpp
)

add_executable(
	${APPNAME_DLL}
	${ROOT_DIR}/src/sample-dll/dll-test.cpp
)

add_executable(
	${APPNAME_NC} 
	${ROOT_DIR}/src/sample-nc/main.cpp
	${ROOT_DIR}/src/utils/sound_file.cpp
	${ROOT_DIR}/src/utils/argument_parser.cpp
)

target_include_directories(
	${APPNAME_NC}
	PRIVATE
	${ROOT_DIR}/src/utils
	${LIBSNDFILE_INC}
	${KRISP_INC_DIR}
)

target_include_directories(
	${DLLNAME}
	PRIVATE
	${KRISP_INC_DIR}
)

target_include_directories(
	${APPNAME_DLL}
	PRIVATE
	${KRISP_INC_DIR}
)

target_link_libraries(
	${APPNAME_NC}
	${LIBSNDFILE_ABSPATH}
	${KRISP_LIBS}
)

target_link_libraries(
	${DLLNAME}
	${KRISP_LIBS}
)

target_link_libraries(
	${APPNAME_DLL}
	${DLLNAME}
)
