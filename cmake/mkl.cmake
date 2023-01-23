##
# Set MKL paths based on platform
#
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(MKL_PATH /opt/intel/mkl)
	if(NOT EXISTS ${MKL_PATH})
		set(MKL_PATH /opt/intel/oneapi/mkl/latest)
		if(NOT EXISTS ${MKL_PATH})
			message(FATAL_ERROR "Wrong MKL path: " ${MKL_PATH} ". Seems like MKL doesn't installed.")
		endif()
	endif()
    set(MKL_LIB_PATH "${MKL_PATH}/lib/intel64")
    set(MKL_DLL_PATH "${MKL_PATH}/lib/intel64")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(MKL_PATH /opt/intel/mkl)
    if(NOT EXISTS ${MKL_PATH})
        set(MKL_PATH /opt/intel/oneapi/mkl/latest)
        if(NOT EXISTS ${MKL_PATH})
            message(FATAL_ERROR "Wrong MKL path: " ${MKL_PATH} ". Seems like MKL doesn't installed.")
        endif()
    endif()
    set(MKL_LIB_PATH "${MKL_PATH}/lib")
    set(MKL_DLL_PATH "${MKL_PATH}/lib")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(MKL_PATH "C:/Program Files (x86)/IntelSWTools/compilers_and_libraries/windows/mkl")
	if(EXISTS ${MKL_PATH})
		if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
			set(MKL_LIB_PATH "${MKL_PATH}/lib/ia32")
			set(MKL_DLL_PATH "${MKL_PATH}/../redist/ia32/mkl")
		else ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
			set(MKL_LIB_PATH "${MKL_PATH}/lib/intel64")
			set(MKL_DLL_PATH "${MKL_PATH}/../redist/intel64/mkl")
		endif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
	else()
		set(MKL_PATH "C:/Program Files (x86)/Intel/oneAPI/mkl/latest")
		if(EXISTS ${MKL_PATH})
			if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
				set(MKL_LIB_PATH "${MKL_PATH}/lib/ia32")
				set(MKL_DLL_PATH "${MKL_PATH}/redist/ia32/")
			else ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
				set(MKL_LIB_PATH "${MKL_PATH}/lib/intel64")
				set(MKL_DLL_PATH "${MKL_PATH}/redist/intel64/")
			endif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")	
		else()
			message(FATAL_ERROR "Wrong MKL path: " ${MKL_PATH})
		endif()
	
	endif()
else()
    message(FATAL_ERROR "Unsupported platform: " ${CMAKE_SYSTEM_NAME})
endif()

##
# Set compiler flags
#
if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMKL_ILP64 -m64")
endif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")


include_directories(${MKL_PATH}/include)

##
# mkl_rt for shared linking
#
find_library(mkl_rt    NAME ${CMAKE_SHARED_LIBRARY_PREFIX}mkl_rt${CMAKE_SHARED_LIBRARY_SUFFIX}          PATHS ${MKL_DLL_PATH})

##
# mkl static libs
#
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
		find_library(mkl_intel_c NAME ${CMAKE_STATIC_LIBRARY_PREFIX}mkl_intel_c${CMAKE_STATIC_LIBRARY_SUFFIX} PATHS ${MKL_LIB_PATH})
	else ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
		find_library(mkl_ilp64 NAME ${CMAKE_STATIC_LIBRARY_PREFIX}mkl_intel_ilp64${CMAKE_STATIC_LIBRARY_SUFFIX} PATHS ${MKL_LIB_PATH})
	endif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
else ()
	find_library(mkl_ilp64 NAME ${CMAKE_STATIC_LIBRARY_PREFIX}mkl_intel_ilp64${CMAKE_STATIC_LIBRARY_SUFFIX} PATHS ${MKL_LIB_PATH})
endif()

find_library(mkl_seq   NAME ${CMAKE_STATIC_LIBRARY_PREFIX}mkl_sequential${CMAKE_STATIC_LIBRARY_SUFFIX}  PATHS ${MKL_LIB_PATH})
find_library(mkl_core  NAME ${CMAKE_STATIC_LIBRARY_PREFIX}mkl_core${CMAKE_STATIC_LIBRARY_SUFFIX}        PATHS ${MKL_LIB_PATH})


##
# mkl_libs for static linking
#
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(mkl_libs -Wl,--start-group ${mkl_ilp64} ${mkl_seq} ${mkl_core} -Wl,--end-group -lpthread -lm -ldl)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(mkl_libs ${mkl_ilp64} ${mkl_seq} ${mkl_core} -lpthread -lm -ldl)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	##
	# Link architecture specific libs
	#
	if ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
		set(mkl_libs ${mkl_intel_c} ${mkl_seq} ${mkl_core})
	else ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
		set(mkl_libs ${mkl_ilp64} ${mkl_seq} ${mkl_core})
	endif ("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Win32")
endif()

