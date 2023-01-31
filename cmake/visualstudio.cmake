# This logic needs to be considered before project()
set(_CHANGE_MSVC_FLAGS FALSE)
if(WIN32 AND NOT MSVC_DYNAMIC_CRT)
	if(CMAKE_VERSION VERSION_LESS 3.15.0)
		set(_CHANGE_MSVC_FLAGS TRUE)
	else()
		# Set MSVC runtime to MultiThreaded (/MT)
		message("-- MSVC_RUNTIME_LIBRARY set to MultiThreaded (/MT)")
		cmake_policy(SET CMP0091 NEW)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
endif()

# Modify compile flags to change MSVC runtime from default /MD to /MT
if(NOT MSVC_DYNAMIC_CRT AND _CHANGE_MSVC_FLAGS)
	message("-- MSVC STATIC CRT is enabled...")
	set(COMPILER_FLAGS
		CMAKE_CXX_FLAGS
		CMAKE_CXX_FLAGS_DEBUG
		CMAKE_CXX_FLAGS_RELEASE
		CMAKE_C_FLAGS
		CMAKE_C_FLAGS_DEBUG
		CMAKE_C_FLAGS_RELEASE
	)
	foreach(COMPILER_FLAG ${COMPILER_FLAGS})
		string(REPLACE "/MD" "/MT" ${COMPILER_FLAG} "${${COMPILER_FLAG}}")
	endforeach()
endif()
