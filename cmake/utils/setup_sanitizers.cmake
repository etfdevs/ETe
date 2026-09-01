# note: Clang is only tested on Linux
# this might not work at all on Apple Clang (or Windows for that matter)
include_guard(GLOBAL)

include(CheckCCompilerFlag)

function(setup_sanitizers)
    if (USE_ASAN)
        setup_asan()
    endif ()

    if (USE_LSAN)
        setup_lsan()
    endif ()

    if (USE_UBSAN)
        setup_ubsan()
    endif ()

    # GCC/Clang need this for better stack traces
    if (NOT CMAKE_C_COMPILER_ID MATCHES "MSVC")
        add_compile_options(-fno-omit-frame-pointer)
    endif ()
endfunction()

function(setup_asan)
    if (CMAKE_C_COMPILER_ID MATCHES "MSVC")
        set(ASAN_COMPILER_FLAGS
                /fsanitize=address)
    elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(ASAN_COMPILER_FLAGS
                -fsanitize=address
                -fsanitize-address-use-after-scope)
    else()
        set(ASAN_COMPILER_FLAGS
                -fsanitize=address,pointer-compare,pointer-subtract
                -fsanitize-address-use-after-scope)
    endif ()


    set(CMAKE_REQUIRED_FLAGS "${ASAN_COMPILER_FLAGS}")
    check_c_compiler_flag("${ASAN_COMPILER_FLAGS}" HAVE_FSANITIZE_ADDRESS_C)

    if (HAVE_FSANITIZE_ADDRESS_C)
        message(STATUS "Enabling address sanitizer for selected configuration")
    else ()
        message(FATAL_ERROR "Cannot enable address sanitizer, unsupported compiler")
    endif ()
    
    add_compile_options("${ASAN_COMPILER_FLAGS}")

    if (CMAKE_C_COMPILER_ID MATCHES "MSVC")
        # for running address sanitizer outside of Visual Studio,
        # the asan runtime libraries must be part of PATH (VS adds this automatically)
        get_filename_component(COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
        message(STATUS "  If running outside of Visual Studio, add the following to your PATH if running un-sanitized executable:")
        message(STATUS "  ${COMPILER_DIR}")
    else ()
        add_link_options("-fsanitize=address")

        # Clang does not use shared asan libraries by default,
        # which is incompatible with -Wl --no-undefined
        if (CMAKE_C_COMPILER_ID MATCHES "Clang")
            add_link_options("-shared-libasan")
        endif ()
    endif ()
endfunction()

function(setup_lsan)
    # MSVC does not support leak sanitizer
    if (CMAKE_C_COMPILER_ID MATCHES "MSVC")
        message(FATAL_ERROR "Leak sanitizer is not supported on MSVC")
    endif ()

    # leak sanitizer is enabled by default with address sanitizer
    if (USE_ASAN)
        if (NOT USE_SANITIZERS)
            message(STATUS "Not running leak sanitizer in standalone mode, exiting")
        endif ()
        return()
    endif ()

    set(LSAN_FLAGS -fsanitize=leak)

    set(CMAKE_REQUIRED_FLAGS "${LSAN_FLAGS}")
    check_c_compiler_flag("${LSAN_FLAGS}" HAVE_FSANITIZE_LEAK_C)

    if (HAVE_FSANITIZE_LEAK_C)
        message(STATUS "Enabling leak sanitizer for selected configuration")
    else ()
        message(FATAL_ERROR "Cannot enable leak sanitizer, unsupported compiler")
    endif ()

    add_compile_options("${LSAN_FLAGS}")
    add_link_options("${LSAN_FLAGS}")
endfunction()

function(setup_ubsan)
    # MSVC does not support undefined behavior sanitizer
    if (CMAKE_C_COMPILER_ID MATCHES "MSVC")
        message(FATAL_ERROR "Undefined behavior sanitizer is not supported on MSVC")
    endif ()

    set(UBSAN_FLAGS -fsanitize=undefined,float-cast-overflow,float-divide-by-zero)

    set(CMAKE_REQUIRED_FLAGS "${UBSAN_FLAGS}")
    check_c_compiler_flag("${UBSAN_FLAGS}" HAVE_FSANITIZE_UB_C)

    if (HAVE_FSANITIZE_UB_C)
        message(STATUS "Enabling undefined behavior sanitizer for selected configuration")
    else ()
        message(FATAL_ERROR "Cannot enable undefined behavior sanitizer, unsupported compiler")
    endif ()

    add_compile_options("${UBSAN_FLAGS}")
    add_link_options("${UBSAN_FLAGS}")
endfunction()
