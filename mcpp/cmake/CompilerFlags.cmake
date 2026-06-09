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
        # WORLDGEN 1:1 CORRECTNESS — NOT optional. Java never fuses a*b+c into an
        # FMA; clang defaults to -ffp-contract=on and will, producing 1-ULP
        # differences that flip noise comparisons at thresholds (e.g. the ore-vein
        # 0.4 cutoff), breaking byte-exact parity with the real generator. Force
        # strict, non-contracted IEEE-754 double evaluation to match the JVM.
        -ffp-contract=off
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # x86-64-v3 (AVX2/BMI2, Haswell+ "modern hardware") improves the noise/density
        # math codegen. FP-safe for byte-exact worldgen parity: scalar IEEE-754 doubles
        # are bit-identical across SSE2/AVX, and with -ffp-contract=off + no -ffast-math
        # clang does not FP-reassociate or auto-vectorize FP reductions, so results are
        # unchanged. Verified by full_chunk_parity (mismatches=0). Override with
        # -DMCPP_ARCH=... if a target CPU lacks AVX2.
        set(MCPP_ARCH "x86-64-v3" CACHE STRING "clang -march for Release")
        add_compile_options(-O3 -flto=thin -march=${MCPP_ARCH})
    else()
        add_compile_options(-O0 -g)
    endif()
    # Link libc++ and libunwind statically so executables run without the
    # llvm-mingw runtime DLLs in PATH. Two separate -Wl, groups: the inner
    # -Wl,-Bdynamic must not be nested inside the first group or lld sees a
    # bare "-Wl" token and errors.
    add_link_options("-Wl,-Bstatic,-lc++,-lunwind" "-Wl,-Bdynamic")
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()
