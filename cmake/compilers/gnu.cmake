# GNU style (GCC/Clang) compiler specific settings

if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
    return()
endif()

include(utils/arch)

enable_language(ASM)

# client sound mixer assembly
if(ARCH MATCHES "x86_64")
	set(ASM_CLIENT_SOURCES
		"${SOURCE_DIR}/asm/snd_mix_x86_64.s"
	)
elseif(ARCH MATCHES "x86")
	set(ASM_CLIENT_SOURCES
		"${SOURCE_DIR}/asm/snd_mix_mmx.s"
        "${SOURCE_DIR}/asm/snd_mix_sse.s"
	)
endif()

set_property(SOURCE ${ASM_CLIENT_SOURCES} APPEND PROPERTY COMPILE_OPTIONS "-DELF")
