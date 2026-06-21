set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This project uses headers, not C++20/23 modules. CMake's module dependency
# scanning (the per-TU .modmap / clang header-unit BMIs) is pure overhead here and
# has miscompiled headers reached via multiple `..` relative paths (spurious
# "redefinition" that even an #ifndef guard can't stop, because the duplicate comes
# from a stale header-unit BMI rather than a textual include). Turn it off globally.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_compile_definitions(
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
    UNICODE _UNICODE
    _USE_MATH_DEFINES
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
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Linux CI / parity-only build (the engine itself targets Windows+MSVC/clang).
    # GCC defaults to -ffp-contract=off which is what byte-exact FP parity needs.
    add_compile_options(
        -Wall -Wno-unused-variable -Wno-missing-field-initializers
        # WORLDGEN 1:1 CORRECTNESS — keep FP strict (no contraction, no fast-math,
        # no reassociation). Same rule as the clang branch above.
        -ffp-contract=off
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O2)
    else()
        add_compile_options(-O0 -g)
    endif()
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()
