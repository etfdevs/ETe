# Clang/AppleClang compiler specific settings

if(NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
    return()
endif()

function(create_compiler_opts target)
	set(WARN_LEVEL 3)

	# parse named arguments
	set(options "")
	set(args WARN)
	set(list_args DEFINE)
	cmake_parse_arguments(PARSE_ARGV 1 arg "${options}" "${args}" "${list_args}")

	# todo validate
	if (NOT ${arg_WARN} STREQUAL "")
		set(WARN_LEVEL ${arg_WARN})	
	endif()

	# Clang flags
	set(CLANG_LINK_FLAGS
		$<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--no-undefined>
		$<$<CONFIG:Release>:
			-flto								# link time optimizations
			-O3									# max optimization
			$<IF:$<PLATFORM_ID:Darwin>,,-s>>)	# strip symbols

	set(CLANG_C_FLAGS
		-pipe
		-fPIC
		-fvisibility=hidden
		-fcolor-diagnostics
		$<IF:$<STREQUAL:${WARN_LEVEL},0>,-w,-Wall -Wdeclaration-after-statement>
		#$<IF:$<STREQUAL:${WARN_LEVEL},0>,-w,-Wall -Wextra -Wpedantic -Wcast-qual -Wdeclaration-after-statement>
		-Winline
		-Wno-unused-parameter
		-Wno-missing-field-initializers
		$<$<CONFIG:Release>:
			-O3					# max optimization
			-flto				# link time optimizations
			-ffast-math			# fast floating point math
			-fomit-frame-pointer
			-finline-functions
			-fno-unsafe-math-optimizations>
		$<$<CONFIG:Debug>:
			-O0					# suppress optimizations
			-g3					# generate debug info
			-ggdb3>)			# generate gdb friendly debug info

	add_library(${target} INTERFACE)

	# we use this to print out relative path to a source code file for __FILE__ macro replacement
	string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
	target_compile_definitions(${target} INTERFACE "SOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

    target_compile_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:C,OBJC>:${CLANG_C_FLAGS}>)
    target_compile_definitions(${target} INTERFACE $<$<COMPILE_LANGUAGE:ASM>:ELF>)
    target_compile_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>)
    target_link_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:C,OBJC>:${CLANG_LINK_FLAGS}>)
	
	target_compile_definitions(${target} INTERFACE 
		$<$<CONFIG:Release>:NDEBUG>
		$<$<CONFIG:Debug>:_DEBUG>
		${arg_DEFINE})

	target_compile_features(${target} INTERFACE c_std_11)
endfunction()
