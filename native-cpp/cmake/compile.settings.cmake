enable_language(C)
enable_language(CXX)

# Set the C++ standard globally
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SYSTEM_NAME MATCHES Linux OR CMAKE_SYSTEM_NAME MATCHES Darwin)
    # GCC and "CLang" specific options
    # add_compile_options(-Werror -Wfatal-errors -Wall -Wextra -Woverloaded-virtual -Wwrite-strings -frtti -fPIC)

    # Add configuration-specific options
    add_compile_options($<$<CONFIG:DEBUG>:-O0>)
    add_compile_options($<$<CONFIG:DEBUG>:-g3>)
    add_compile_options($<$<CONFIG:DEBUG>:-DDEBUG>)
    add_compile_options($<$<CONFIG:RELEASE>:-O3>)

    if(CMAKE_SYSTEM_NAME MATCHES Darwin)
        add_compile_options(-Wno-deprecated -Wno-documentation -Wno-documentation-unknown-command)

        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")
        add_compile_options(-stdlib=libc++)

        if(CMAKE_SYSTEM_PROCESSOR MATCHES arm64)
            set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES amd64)
            add_compile_options(-m64)
            set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "" FORCE)
        endif()

        set(CMAKE_MACOSX_RPATH TRUE)
        set(CMAKE_INSTALL_RPATH "@loader_path;@executable_path")
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES Linux)
        set(CMAKE_INSTALL_RPATH "\$ORIGIN")
        add_link_options(-ldl)

        if(CMAKE_SYSTEM_PROCESSOR MATCHES x86_64)
            add_compile_options(-m64)
        endif()
    endif()

elseif(CMAKE_SYSTEM_NAME MATCHES Windows)
    # MSVC specific options
    add_compile_options(/Zc:__cplusplus /MP /W3 /WX /EHsc /wd4251 /wd4996)
    add_compile_options($<$<CONFIG:DEBUG>:/Od>)
    add_compile_options($<$<CONFIG:DEBUG>:/DEBUG>)
    add_compile_options($<$<CONFIG:RELEASE>:/O2>)
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " /ignore:4099")

    if(MSVC_LTCG)
        add_compile_options("$<$<CONFIG:Release>:/GL>")
        add_link_options("$<$<CONFIG:Release>:/LTCG>")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64)
        set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "" FORCE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ARM64)
        set(CMAKE_GENERATOR_PLATFORM ARM64 CACHE STRING "" FORCE)
    else()
        set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "" FORCE)
    endif()
    # Set _ITERATOR_DEBUG_LEVEL based on configuration using generator expressions
    # This ensures it's set correctly for each configuration when using Visual Studio generators
    # Debug profiles typically use _ITERATOR_DEBUG_LEVEL=2
    add_compile_options("$<$<CONFIG:Debug>:/D_ITERATOR_DEBUG_LEVEL=2>")
    # Release profiles must use _ITERATOR_DEBUG_LEVEL=0 (not supported > 1 in release mode)
    add_compile_options("$<$<CONFIG:Release>:/D_ITERATOR_DEBUG_LEVEL=0>")
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:/D_ITERATOR_DEBUG_LEVEL=0>")
    add_compile_options("$<$<CONFIG:MinSizeRel>:/D_ITERATOR_DEBUG_LEVEL=0>")
    message(STATUS "Setting _ITERATOR_DEBUG_LEVEL: Debug=2, Release/RelWithDebInfo/MinSizeRel=0")
elseif(CMAKE_SYSTEM_NAME MATCHES iOS)
    add_compile_options($<$<CONFIG:RELEASE>:-O3>)
elseif(CMAKE_SYSTEM_NAME MATCHES Android)
    # Android specific options
    add_compile_options($<$<CONFIG:DEBUG>:-O0>)
    add_compile_options($<$<CONFIG:DEBUG>:-DDEBUG>)
    add_compile_options($<$<CONFIG:RELEASE>:-O3>)
    add_link_options($<$<CONFIG:RELEASE>:-s>)
else()
    message(FATAL_ERROR "Unsupported CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
endif()
