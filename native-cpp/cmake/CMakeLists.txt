cmake_minimum_required(VERSION 3.10)

# Set MSVC runtime library policy before project() - this must be done early
if(WIN32)
	cmake_policy(SET CMP0091 NEW)
	# Allow CMAKE_MSVC_RUNTIME_LIBRARY to be set via command line
	# If not set, default to static runtime to match Krisp SDK libraries
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
	endif()
	message(STATUS "MSVC runtime library will be set to: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
endif()

include(compile.settings.cmake)

project("Krisp SDK Sample Apps")

if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

get_filename_component(ROOT_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ROOT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ROOT_DIR}/bin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX OR APPLE)
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
			-Wno-undefined-func-template
		)
    endif()
endif()

# Option to copy SDK to project root for easier IDE integration
set(COPY_KRISP_SDK "" CACHE STRING "Path to Krisp SDK to copy")
option(BUILD_SAMPLE_NC "Build sample-nc" OFF)
option(BUILD_SAMPLE_VT "Build sample-vt" OFF)
option(BUILD_WAV_CLI "Build krisp-wav app" OFF)
option(BUILD_SAMPLE_VAD "Build sample-vad" OFF)
option(BUILD_SAMPLE_AL "Build sample-ar" OFF)
option(BUILD_SAMPLE_PYTHON "Build sample-python" OFF)
option(BUILD_SAMPLE_NODEJS "Build sample-nodejs" OFF)
option(ENABLE_ACCENT "Enable Accent API" OFF)
option(ENABLE_LICENSING "Enable Licensing" OFF)
option(USE_KRISP_DYNAMIC_LIBRARY "Use Krisp dynamic library" OFF)

# Configure Krisp SDK
set(KRISP_SDK_LOCAL ${CMAKE_BINARY_DIR}/../krisp-sdk)
get_filename_component(KRISP_SDK_LOCAL "${KRISP_SDK_LOCAL}" ABSOLUTE)

if(COPY_KRISP_SDK)
	if(EXISTS ${KRISP_SDK_LOCAL})
		message(FATAL_ERROR "Krisp SDK already exists at: ${KRISP_SDK_LOCAL}")
	endif()
	file(MAKE_DIRECTORY ${KRISP_SDK_LOCAL})
    file(COPY ${COPY_KRISP_SDK}/ DESTINATION ${KRISP_SDK_LOCAL})
    message(STATUS "Krisp SDK copied to: ${KRISP_SDK_LOCAL}")
endif()

if(NOT EXISTS ${KRISP_SDK_LOCAL})
	message(FATAL_ERROR "KRISP_SDK_LOCAL directory does not exist: ${KRISP_SDK_LOCAL}")
endif()

set(KRISP_INC_DIR ${KRISP_SDK_LOCAL}/include)
if (USE_KRISP_DYNAMIC_LIBRARY)
	set(KRISP_LIB_DIR ${KRISP_SDK_LOCAL}/lib/dynamic)
else()
	set(KRISP_LIB_DIR ${KRISP_SDK_LOCAL}/lib/static)
	set(KRISP_3PARTY_LIB_DIR ${KRISP_SDK_LOCAL}/lib/static/external)
endif()

# Krisp SDK libraries are applied to all targets
include(krisp.cmake)

if (WIN32)
	add_compile_definitions(KRISP_AUDIO_STATIC)
	add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
	if (MSVC)
		# Explicitly set runtime library flags based on CMAKE_MSVC_RUNTIME_LIBRARY
		# CMAKE_MSVC_RUNTIME_LIBRARY doesn't always work reliably with Visual Studio generators
		# So we explicitly set the compiler flags to ensure correct runtime
		if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded" AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
			# Static runtime: MultiThreaded or MultiThreadedDebug
			# Use generator expressions to set runtime library flags per configuration
			add_compile_options(
				"$<$<CONFIG:Debug>:/MTd>"
				"$<$<CONFIG:Release>:/MT>"
				"$<$<CONFIG:RelWithDebInfo>:/MT>"
				"$<$<CONFIG:MinSizeRel>:/MT>"
			)
			# Also remove any conflicting /MD flags that might be set
			string(REPLACE "/MDd" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
			string(REPLACE "/MD" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
			string(REPLACE "/MDd" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
			string(REPLACE "/MD" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
			message(STATUS "Forcing static runtime: Debug=/MTd, Release/RelWithDebInfo/MinSizeRel=/MT")
		elseif(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreadedDLL")
			# Dynamic runtime: MultiThreadedDLL or MultiThreadedDebugDLL
			# Use generator expressions to set runtime library flags per configuration
			add_compile_options(
				"$<$<CONFIG:Debug>:/MDd>"
				"$<$<CONFIG:Release>:/MD>"
				"$<$<CONFIG:RelWithDebInfo>:/MD>"
				"$<$<CONFIG:MinSizeRel>:/MD>"
			)
			# Also remove any conflicting /MT flags that might be set
			string(REPLACE "/MTd" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
			string(REPLACE "/MT" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
			string(REPLACE "/MTd" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
			string(REPLACE "/MT" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
			message(STATUS "Forcing dynamic runtime: Debug=/MDd, Release/RelWithDebInfo/MinSizeRel=/MD")
		endif()
	endif()
endif()

if (BUILD_SAMPLE_NC)
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
		#/Users/atatalyan/jemalloc-install/lib/libjemalloc.a
	)
endif ()

if (BUILD_SAMPLE_VT)
	set(APPNAME_VT sample-vt)
	# is needed to get ${LIBSNDFILE_ABSPATH} and ${LIBSNDFILE_INC}
	include(libsndfile.cmake)
	add_executable(
		${APPNAME_VT}
		${ROOT_DIR}/src/sample-vt/main.cpp
		${ROOT_DIR}/src/utils/sound_file.cpp
		${ROOT_DIR}/src/utils/argument_parser.cpp
	)
	target_include_directories(
		${APPNAME_VT}
		PRIVATE
		${ROOT_DIR}/src/utils
		${LIBSNDFILE_INC}
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${APPNAME_VT}
		${LIBSNDFILE_ABSPATH}
		${KRISP_LIBS}
		#/Users/atatalyan/jemalloc-install/lib/libjemalloc.a
	)
	if(APPLE)
		# Set rpath so the executable can find the dynamic library at runtime
		set_target_properties(${APPNAME_VT} PROPERTIES
			BUILD_WITH_INSTALL_RPATH TRUE
			INSTALL_RPATH "${KRISP_3PARTY_LIB_DIR}"
		)
	endif()
endif ()

if (BUILD_WAV_CLI)
	set(APPNAME_WAV_CLI krisp-wav-cli)
	add_executable(
		${APPNAME_WAV_CLI}
		${ROOT_DIR}/src/wav-cli/main.cpp
		${ROOT_DIR}/src/utils/argument_parser.cpp
	)
	# Ensure runtime library matches what the linked libraries expect (static runtime)
	# The runtime library should already be set globally via CMAKE_MSVC_RUNTIME_LIBRARY,
	# but we explicitly set it on the target to ensure it's applied
	if(WIN32 AND MSVC)
		set_target_properties(${APPNAME_WAV_CLI} PROPERTIES
			MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}"
		)
		message(STATUS "Target ${APPNAME_WAV_CLI} runtime library: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
	endif()
	if (ENABLE_ACCENT)
		set_target_properties(${APPNAME_WAV_CLI} PROPERTIES COMPILE_DEFINITIONS "ENABLE_ACCENT")
	endif()
	if (ENABLE_LICENSING)
		set_target_properties(${APPNAME_WAV_CLI} PROPERTIES COMPILE_DEFINITIONS "ENABLE_LICENSING")
	endif()
	target_include_directories(
		${APPNAME_WAV_CLI}
		PRIVATE
		${ROOT_DIR}/src/utils
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${APPNAME_WAV_CLI}
		${KRISP_LIBS}
	)
endif ()

if (BUILD_SAMPLE_VAD)
	set(APPNAME_VAD sample-vad)
	# is needed to get ${LIBSNDFILE_ABSPATH} and ${LIBSNDFILE_INC}
	include(libsndfile.cmake)
	add_executable(
		${APPNAME_VAD}
		${ROOT_DIR}/src/sample-vad/main.cpp
		${ROOT_DIR}/src/utils/sound_file.cpp
		${ROOT_DIR}/src/utils/argument_parser.cpp
	)
	target_include_directories(
		${APPNAME_VAD}
		PRIVATE
		${ROOT_DIR}/src/utils
		${LIBSNDFILE_INC}
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${APPNAME_VAD}
		${LIBSNDFILE_ABSPATH}
		${KRISP_LIBS}
	)
endif ()

if (BUILD_SAMPLE_AL)
	set(APPNAME_AR sample-ar)
	include(libsndfile.cmake)
	add_executable(
		${APPNAME_AR} 
		${ROOT_DIR}/src/sample-ar/main.cpp
		${ROOT_DIR}/src/utils/sound_file.cpp
		${ROOT_DIR}/src/utils/argument_parser.cpp
	)
	target_include_directories(
		${APPNAME_AR}
		PRIVATE
		${ROOT_DIR}/src/utils
		${LIBSNDFILE_INC}
		${KRISP_INC_DIR}
	)
	target_link_libraries(
		${APPNAME_AR}
		${KRISP_LIBS}
		${LIBSNDFILE_ABSPATH}
	)
endif ()

if (BUILD_SAMPLE_PYTHON)
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
		${ROOT_DIR}/src/utils
	)
	target_link_libraries(
		${PYMODNAME_NC}
		PRIVATE Python3::NumPy
		${KRISP_LIBS}
	)
endif ()

if (BUILD_SAMPLE_NODEJS)
	if (NOT NODE_INC)
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
		${ROOT_DIR}/src/utils
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
