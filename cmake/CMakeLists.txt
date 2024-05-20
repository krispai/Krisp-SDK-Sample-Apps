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
set(KRISP_INC_LEGACY_DIR ${KRISP_SDK_PATH}/include)
set(KRISP_LIB_DIR ${KRISP_SDK_PATH}/lib)
set(KRISP_3PARTY_LIB_DIR ${KRISP_SDK_PATH}/external)

# is needed to get ${LIBSNDFILE_ABSPATH} and ${LIBSNDFILE_INC}
include(libsndfile.cmake)

# Krisp SDK libraries are applied to all targets
include(krisp.cmake)

set(APPNAME_STT sample-stt)
set(APPNAME_NC sample-nc)
set(DLLNAME krisp-dll)
if (NOT WIN32)
	set(APPNAME_DLL dll-test-app)
endif()
#set(APPNAME_VAD sample-vad)

add_compile_definitions(KRISP_AUDIO_STATIC)
if (WIN32 AND MSVC)
	add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

add_library(
	${DLLNAME} SHARED
	${ROOT_DIR}/src/sample-dll/dll-main.cpp
)

if (NOT WIN32)
	add_executable(
		${APPNAME_DLL}
		${ROOT_DIR}/src/sample-dll/dll-test.cpp
	)
endif()

add_executable(
	${APPNAME_NC} 
	${ROOT_DIR}/src/sample-nc/main.cpp
	${ROOT_DIR}/src/utils/sound_file.cpp
	${ROOT_DIR}/src/utils/argument_parser.cpp
)

if (DEFINED STT)
	add_executable(
		${APPNAME_STT} 
		${ROOT_DIR}/src/sample-stt/main.cpp
		${ROOT_DIR}/src/utils/sound_file.cpp
		${ROOT_DIR}/src/utils/argument_parser.cpp
	)
endif()

target_include_directories(
	${APPNAME_NC}
	PRIVATE
	${ROOT_DIR}/src/utils
	${LIBSNDFILE_INC}
	${KRISP_INC_LEGACY_DIR}
)

if (DEFINED STT)
	target_include_directories(
		${APPNAME_STT}
		PRIVATE
		${ROOT_DIR}/src/utils
		${LIBSNDFILE_INC}
		${KRISP_INC_LEGACY_DIR}
	)
endif()

target_include_directories(
	${DLLNAME}
	PRIVATE
	${KRISP_INC_LEGACY_DIR}
)

if (NOT WIN32)
	target_include_directories(
		${APPNAME_DLL}
		PRIVATE
		${KRISP_INC_LEGACY_DIR}
	)
endif()

target_link_libraries(
	${APPNAME_NC}
	${KRISP_LIBS}
	${LIBSNDFILE_ABSPATH}
)

if (DEFINED STT)
	target_link_libraries(
		${APPNAME_STT}
		${KRISP_LIBS}
		${LIBSNDFILE_ABSPATH}
	)
endif()

target_link_libraries(
	${DLLNAME}
	${KRISP_LIBS}
)

if (NOT WIN32)
	target_link_libraries(
		${APPNAME_DLL}
		${DLLNAME}
	)
endif()
