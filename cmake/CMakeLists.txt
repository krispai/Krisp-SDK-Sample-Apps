cmake_minimum_required(VERSION 3.5)

if (WIN32)
	include(visualstudio.cmake NO_POLICY_SCOPE)
endif()

project("Krisp SDK Sample Apps")

# Set 'Release' or 'Debug'
set(CMAKE_BUILD_TYPE Release)

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

if (DEFINED BUILD_SAMPLE_PYTHON AND BUILD_SAMPLE_PYTHON STREQUAL "1")
	if (DEFINED PYTHON3_PATH AND NOT "PYTHON3_PATH" STREQUAL "")
		set(Python3_EXECUTABLE "${PYTHON3_PATH}")
	endif()
	find_package(Python3 REQUIRED COMPONENTS Development NumPy)
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

if (DEFINED BUILD_SAMPLE_NODEJS AND BUILD_SAMPLE_NODEJS STREQUAL "1")
	if (NOT NODE_INC OR NODE_INC STREQUAL "")
		message(FATAL_ERROR "the NODE_INC is not set.")
	endif()
	set(NODEJS_SAMPLE_DIR ${ROOT_DIR}/src/sample-node)
	execute_process(
		COMMAND npm install
		WORKING_DIRECTORY ${NODEJS_SAMPLE_DIR}
		RESULT_VARIABLE NPM_INSTALL_RESULT
		ERROR_VARIABLE NPM_INSTALL_ERROR
	)
	if (NOT NPM_INSTALL_RESULT EQUAL 0)
		message(FATAL_ERROR "npm install failed with error: ${NPM_INSTALL_ERROR}")
	endif()
	execute_process(
		COMMAND node -p "require('node-addon-api').include"
		WORKING_DIRECTORY ${NODEJS_SAMPLE_DIR}
		OUTPUT_VARIABLE NODE_ADDON_API_DIR
		RESULT_VARIABLE NODE_ADDON_API_RESULT
		ERROR_VARIABLE NODE_ADDON_API_ERROR
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	string(STRIP "${NODE_ADDON_API_DIR}" NODE_ADDON_API_DIR)
	string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")
	if (NOT NODE_ADDON_API_RESULT EQUAL 0 OR NODE_ADDON_API_DIR STREQUAL "")
		message(FATAL_ERROR "Failed to retrieve node-addon-api include directory: ${NODE_ADDON_API_ERROR}")
	endif()
	add_definitions(-DNAPI_VERSION=9)
	set(NODEMODNAME_NC nodejs-module)
	add_library(${NODEMODNAME_NC} SHARED ${ROOT_DIR}/src/sample-node/addon.cpp)
	if (APPLE)
		set_target_properties(
			${NODEMODNAME_NC} PROPERTIES
			LINK_FLAGS "-undefined dynamic_lookup"
		)
	endif()
	target_link_libraries(${NODEMODNAME_NC} PRIVATE ${KRISP_LIBS})
	target_include_directories(
		${NODEMODNAME_NC}
		PRIVATE
		${NODE_INC}
		${NODE_ADDON_API_DIR}
		${KRISP_INC_DIR}
	)
	set_target_properties(
		${NODEMODNAME_NC} PROPERTIES
		PREFIX ""
		SUFFIX ".node"
        RUNTIME_OUTPUT_DIRECTORY ${NODEJS_SAMPLE_DIR}
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${NODEJS_SAMPLE_DIR}"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${NODEJS_SAMPLE_DIR}"
	)
endif()
