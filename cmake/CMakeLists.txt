cmake_minimum_required(VERSION 3.12)

if(WIN32)
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
    #add_compile_options(-Wall -Werror -pedantic)
    add_compile_options(-Wall -pedantic)

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

if(NOT DEFINED KRISP_SDK_PATH)
	message(FATAL_ERROR "KRISP_SDK_PATH must be specified")
endif()

set(KRISP_INC_DIR ${KRISP_SDK_PATH}/include)
set(KRISP_LIB_DIR ${KRISP_SDK_PATH}/lib)

if(UNIX AND NOT APPLE)
	set(KRISP_LIB_DIR ${KRISP_LIB_DIR}/x64)
elseif(WIN32)
	set(KRISP_LIB_DIR ${KRISP_LIB_DIR}/x64/Release)
endif()

# Krisp SDK libraries are applied to all targets
include(krisp.cmake)

set(APPNAME_NC sample-nc)
set(DLLNAME krisp-dll)
set(APPNAME_DLL dll-test-app)
set(MODNAME_NODE nodejs-module)

add_compile_definitions(KRISP_AUDIO_STATIC)
if (WIN32 AND MSVC)
	add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

if (DEFINED BUILD_SAMPLE_NC AND BUILD_SAMPLE_NC STREQUAL "1")
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

if (DEFINED BUILD_SHARED_LIBRARY AND BUILD_SHARED_LIBRARY STREQUAL "1")
	add_library(
		${DLLNAME} SHARED
		${ROOT_DIR}/src/sample-dll/dll-main.cpp
	)
	target_include_directories(
		${DLLNAME}
		PRIVATE
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${DLLNAME}
		${KRISP_LIBS}
	)
	add_executable(
		${APPNAME_DLL}
		${ROOT_DIR}/src/sample-dll/dll-test.cpp
	)
	target_include_directories(
		${APPNAME_DLL}
		PRIVATE
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${APPNAME_DLL}
		${DLLNAME}
	)
endif ()

if (DEFINED BUILD_PYTHON_SAMPLE AND BUILD_PYTHON_SAMPLE STREQUAL "1")
	find_package(Python3 COMPONENTS Development NumPy)
	find_package(pybind11 REQUIRED)
	pybind11_add_module(
		audio_processor
		${ROOT_DIR}/src/sample-python/audio_processor.cpp
	)
	target_include_directories(
		audio_processor
		PRIVATE
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		audio_processor
		PRIVATE Python3::NumPy
		${KRISP_LIBS}
	)
endif ()

if (DEFINED BUILD_NODEJS_SAMPLE AND BUILD_NODEJS_SAMPLE STREQUAL "1")
	if (NOT NODE_INCLUDE_DIR OR NODE_INCLUDE_DIR STREQUAL "")
		message(FATAL_ERROR "the NODE_INCLUDE_DIR is not set.")
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
	add_library(${MODNAME_NODE} SHARED ${ROOT_DIR}/src/sample-node/addon.cpp)
	if (APPLE)
		set_target_properties(
			${MODNAME_NODE} PROPERTIES
			LINK_FLAGS "-undefined dynamic_lookup"
		)
	else()
	endif()
	target_link_libraries(${MODNAME_NODE} PRIVATE ${KRISP_LIBS})
	target_include_directories(
		${MODNAME_NODE}
		PRIVATE
		${NODE_INCLUDE_DIR}
		${NODE_ADDON_API_DIR}
		${KRISP_INC_DIR}
	)
	set_target_properties(
		${MODNAME_NODE} PROPERTIES
		PREFIX ""
		SUFFIX ".node"
        RUNTIME_OUTPUT_DIRECTORY ${NODEJS_SAMPLE_DIR}
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${NODEJS_SAMPLE_DIR}"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${NODEJS_SAMPLE_DIR}"
	)
endif()

if(UNIX AND NOT APPLE)
	include(mkl.linux.cmake)
endif()
