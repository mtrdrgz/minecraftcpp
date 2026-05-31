set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    UNICODE _UNICODE
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W3 /MP /utf-8)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /GL)
        add_link_options(/LTCG)
    else()
        add_compile_options(/Od /Zi /RTC1)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall -Wno-unused-variable -Wno-missing-field-initializers
        -fcolor-diagnostics
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -flto=thin)
    else()
        add_compile_options(-O0 -g)
    endif()
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()
