# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("krisptestapp")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED KRISP_SDK_ROOT)
    message(FATAL_ERROR "KRISP_SDK_ROOT not set")
endif()

# Include Krisp headers
include_directories(${KRISP_SDK_ROOT}/include)

# ABI-specific lib paths
if (ANDROID_ABI STREQUAL "arm64-v8a")
    set(KRISP_LIB_DIR ${KRISP_SDK_ROOT}/lib/arm64-v8a/static)
elseif (ANDROID_ABI STREQUAL "armeabi-v7a")
    set(KRISP_LIB_DIR ${KRISP_SDK_ROOT}/lib/armeabi-v7a/static)
elseif (ANDROID_ABI STREQUAL "x86_64")
    set(KRISP_LIB_DIR ${KRISP_SDK_ROOT}/lib/x86_64/static)
else()
    message(FATAL_ERROR "Unsupported ABI: ${ANDROID_ABI}")
endif()

add_library(krisp-audio-sdk STATIC IMPORTED)
set_target_properties(krisp-audio-sdk PROPERTIES
        IMPORTED_LOCATION ${KRISP_LIB_DIR}/libkrisp-audio-sdk.a
)

file(GLOB KRISP_EXTERNAL_LIBS "${KRISP_LIB_DIR}/external/*.a")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
    # List C/C++ source files with relative paths to this CMakeLists.txt.
        krisp.cpp)


target_link_libraries(${CMAKE_PROJECT_NAME}
    krisp-audio-sdk
    ${KRISP_EXTERNAL_LIBS}
    android
    log)