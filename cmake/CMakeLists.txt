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
	add_compile_options(-Wall -pedantic -Wno-undefined-func-template)

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

# Krisp SDK libraries are applied to all targets
include(krisp.cmake)

if (WIN32)
	add_compile_definitions(KRISP_AUDIO_STATIC)
endif()
if (WIN32 AND MSVC)
	add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

if (DEFINED BUILD_SAMPLE_NC AND BUILD_SAMPLE_NC STREQUAL "1")
	set(APPNAME_NC sample-nc)
	# is needed to get ${LIBSNDFILE_ABSPATH} and ${LIBSNDFILE_INC}
	include(libsndfile.cmake)
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
	target_link_libraries(
		${APPNAME_NC}
		${LIBSNDFILE_ABSPATH}
		${KRISP_LIBS}
	)
endif ()

if (DEFINED BUILD_SAMPLE_AL AND BUILD_SAMPLE_AL STREQUAL "1")
	set(APPNAME_AL sample-al)
	include(libsndfile.cmake)
	add_executable(
		${APPNAME_AL} 
		${ROOT_DIR}/src/sample-al/main.cpp
		${ROOT_DIR}/src/utils/sound_file.cpp
		${ROOT_DIR}/src/utils/argument_parser.cpp
	)
	target_include_directories(
		${APPNAME_AL}
		PRIVATE
		${ROOT_DIR}/src/utils
		${LIBSNDFILE_INC}
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${APPNAME_AL}
		${KRISP_LIBS}
		${LIBSNDFILE_ABSPATH}
	)
endif ()

if (DEFINED BUILD_PYTHON_SAMPLE AND BUILD_PYTHON_SAMPLE STREQUAL "1")
	find_package(Python3 COMPONENTS Development NumPy)
	find_package(pybind11 REQUIRED)
	set(PYMODNAME_NC krisp_module)
	pybind11_add_module(
		${PYMODNAME_NC}
		${ROOT_DIR}/src/sample-python/krisp_python_module.cpp
	)
	target_include_directories(
		${PYMODNAME_NC}
		PRIVATE
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${PYMODNAME_NC}
		PRIVATE Python3::NumPy
		${KRISP_LIBS}
	)
endif ()
