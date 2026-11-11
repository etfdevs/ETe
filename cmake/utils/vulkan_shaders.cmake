# Vulkan shader data generation.
#
# Compiles the GLSL shaders under src/renderervk/shaders into SPIR-V and
# embeds the result in a generated shader_data.c (plus shader_binding.c).
# Every shader variant is its own custom command, so the build tool is free
# to compile them in parallel. A final custom command concatenates the
# results into the build tree.
#
# A host glslangValidator (Vulkan SDK) is required. If it is not available,
# or USE_VULKAN_SHADER_COMPILE is OFF, the checked-in
# src/renderervk/shaders/spirv/shader_data.c is used instead.

include_guard(GLOBAL)

set(VULKAN_SHADER_SOURCE_DIR "${SOURCE_DIR}/renderervk/shaders")

find_program(VULKAN_GLSLANG_VALIDATOR
    NAMES glslangValidator glslangValidator.exe
    HINTS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{VULKAN_SDK}/bin"
        "$ENV{VULKAN_SDK}/Bin32"
    DOC "Path to the glslangValidator executable (Vulkan SDK)"
)

if(VULKAN_GLSLANG_VALIDATOR)
    set(_vulkan_shader_compile_default ON)
else()
    set(_vulkan_shader_compile_default OFF)
endif()

option(USE_VULKAN_SHADER_COMPILE "Regenerate Vulkan SPIR-V shader data at build time" ${_vulkan_shader_compile_default})
option(USE_VULKAN_SHADER_BINDINGS "Generate shader_binding.c for a future binding-style shader integration" OFF)

# Append a shader variant to the global task list.
# Fields: <c array name>|<input file>|<stage>|<defines>|<binding expression>
function(_vulkan_shader_add_task name input stage defines binding)
    if(NOT binding)
        set(binding "-")
    endif()
    set_property(GLOBAL APPEND PROPERTY VULKAN_SHADER_TASKS "${name}|${input}|${stage}|${defines}|${binding}")
endfunction()

# Join a list of optional define arguments into a single space separated string.
function(_vulkan_shader_join_defines out_var)
    set(parts ${ARGN})
    list(REMOVE_ITEM parts "")
    list(JOIN parts " " joined)
    set(${out_var} "${joined}" PARENT_SCOPE)
endfunction()

function(_vulkan_shader_individual_binding stem out_var)
    set(bindings
        "blend_frag|vk.modules.blend_fs"
        "bloom_frag|vk.modules.bloom_fs"
        "blur_frag|vk.modules.blur_fs"
        "color_frag|vk.modules.color_fs"
        "color_vert|vk.modules.color_vs"
        "dot_frag|vk.modules.dot_fs"
        "dot_vert|vk.modules.dot_vs"
        "fog_frag|vk.modules.fog_fs"
        "fog_vert|vk.modules.fog_vs"
        "gamma_frag|vk.modules.gamma_fs"
        "gamma_vert|vk.modules.gamma_vs"
    )

    foreach(entry IN LISTS bindings)
        string(REPLACE "|" ";" fields "${entry}")
        list(GET fields 0 entry_stem)
        list(GET fields 1 entry_binding)
        if("${entry_stem}" STREQUAL "${stem}")
            set(${out_var} "${entry_binding}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${out_var} "" PARENT_SCOPE)
endfunction()

# Individual .vert/.frag shaders, discovered from the source directory so
# new shaders are picked up automatically.
function(_vulkan_shader_add_individual_variants)
    file(GLOB individual_shaders CONFIGURE_DEPENDS
        "${VULKAN_SHADER_SOURCE_DIR}/*.vert"
        "${VULKAN_SHADER_SOURCE_DIR}/*.frag"
    )
    list(SORT individual_shaders)

    foreach(shader IN LISTS individual_shaders)
        get_filename_component(extension "${shader}" EXT)
        get_filename_component(stem "${shader}" NAME_WE)

        if(extension STREQUAL ".vert")
            set(stage "vert")
        elseif(extension STREQUAL ".frag")
            set(stage "frag")
        else()
            continue()
        endif()

        _vulkan_shader_individual_binding("${stem}_${stage}" binding)
        _vulkan_shader_add_task("${stem}_${stage}_spv" "${stem}${extension}" "${stage}" "" "${binding}")
    endforeach()
endfunction()

# Template shader variations. Keep this matrix in sync with the defines used
# by gen_vert.tmpl / gen_frag.tmpl / light_vert.tmpl / light_frag.tmpl.
function(_vulkan_shader_add_template_variants)
    set(mode_flags "-DUSE_CLX_IDENT" "-DUSE_FIXED_COLOR")
    set(mode_ids "ident1" "fixed")

    set(tx_flags "" "-DUSE_TX1" "-DUSE_TX2")
    set(tx_ids "tx0" "tx1" "tx2")

    set(cl_flags "" "-DUSE_CL1" "-DUSE_CL2")
    set(cl_ids "" "cl" "cl")

    set(env_flags "" "-DUSE_ENV")
    set(env_ids "" "_env")

    set(fog_flags "" "-DUSE_FOG")
    set(fog_ids "" "_fog")

    # Specialized template shaders.
    _vulkan_shader_add_task("frag_tx0_df" "gen_frag.tmpl" "frag"
        "-DUSE_CLX_IDENT -DUSE_ATEST -DUSE_DF" "vk.modules.frag.gen0_df")
    _vulkan_shader_add_task("frag_tx0_ent" "gen_frag.tmpl" "frag"
        "-DUSE_ENT_COLOR -DUSE_ATEST" "vk.modules.frag.ent[0][0]")
    _vulkan_shader_add_task("frag_tx0_ent_fog" "gen_frag.tmpl" "frag"
        "-DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG" "vk.modules.frag.ent[0][1]")

    _vulkan_shader_add_task("vert_light" "light_vert.tmpl" "vert"
        "" "vk.modules.vert.light[0]")
    _vulkan_shader_add_task("vert_light_fog" "light_vert.tmpl" "vert"
        "-DUSE_FOG" "vk.modules.vert.light[1]")

    _vulkan_shader_add_task("frag_light" "light_frag.tmpl" "frag"
        "" "vk.modules.frag.light[0][0]")
    _vulkan_shader_add_task("frag_light_fog" "light_frag.tmpl" "frag"
        "-DUSE_FOG" "vk.modules.frag.light[0][1]")
    _vulkan_shader_add_task("frag_light_line" "light_frag.tmpl" "frag"
        "-DUSE_LINE" "vk.modules.frag.light[1][0]")
    _vulkan_shader_add_task("frag_light_line_fog" "light_frag.tmpl" "frag"
        "-DUSE_LINE -DUSE_FOG" "vk.modules.frag.light[1][1]")

    _vulkan_shader_add_task("vert_light_dir" "light_vert.tmpl" "vert"
        "-DUSE_DIRECTED" "vk.modules.vert.light_dir[0]")
    _vulkan_shader_add_task("vert_light_dir_fog" "light_vert.tmpl" "vert"
        "-DUSE_DIRECTED -DUSE_FOG" "vk.modules.vert.light_dir[1]")

    # Identity / fixed-color vertex shaders.
    foreach(tx_index RANGE 0 1)
        list(GET tx_flags ${tx_index} tx_flag)
        list(GET tx_ids ${tx_index} tx_id)
        foreach(mode_index RANGE 0 1)
            list(GET mode_flags ${mode_index} mode_flag)
            list(GET mode_ids ${mode_index} mode_id)
            foreach(env_index RANGE 0 1)
                list(GET env_flags ${env_index} env_flag)
                list(GET env_ids ${env_index} env_id)
                foreach(fog_index RANGE 0 1)
                    list(GET fog_flags ${fog_index} fog_flag)
                    list(GET fog_ids ${fog_index} fog_id)

                    _vulkan_shader_join_defines(defines "${tx_flag}" "${mode_flag}" "${env_flag}" "${fog_flag}")
                    _vulkan_shader_add_task(
                        "vert_${tx_id}_${mode_id}${env_id}${fog_id}"
                        "gen_vert.tmpl" "vert" "${defines}"
                        "vk.modules.vert.${mode_id}[${tx_index}][${env_index}][${fog_index}]")
                endforeach()
            endforeach()
        endforeach()
    endforeach()

    # Identity / fixed-color fragment shaders.
    foreach(tx_index RANGE 0 1)
        list(GET tx_flags ${tx_index} tx_flag)
        list(GET tx_ids ${tx_index} tx_id)
        foreach(mode_index RANGE 0 1)
            list(GET mode_flags ${mode_index} mode_flag)
            list(GET mode_ids ${mode_index} mode_id)
            foreach(fog_index RANGE 0 1)
                list(GET fog_flags ${fog_index} fog_flag)
                list(GET fog_ids ${fog_index} fog_id)

                if(tx_index EQUAL 0)
                    set(atest_flag "-DUSE_ATEST")
                else()
                    set(atest_flag "")
                endif()

                _vulkan_shader_join_defines(defines "${tx_flag}" "${mode_flag}" "${atest_flag}" "${fog_flag}")
                _vulkan_shader_add_task(
                    "frag_${tx_id}_${mode_id}${fog_id}"
                    "gen_frag.tmpl" "frag" "${defines}"
                    "vk.modules.frag.${mode_id}[${tx_index}][${fog_index}]")
            endforeach()
        endforeach()
    endforeach()

    # Generic vertex shaders.
    foreach(tx_index RANGE 0 2)
        list(GET tx_flags ${tx_index} tx_flag)
        list(GET tx_ids ${tx_index} tx_id)
        list(GET cl_flags ${tx_index} cl_flag)
        list(GET cl_ids ${tx_index} cl_id)
        foreach(env_index RANGE 0 1)
            list(GET env_flags ${env_index} env_flag)
            list(GET env_ids ${env_index} env_id)
            foreach(fog_index RANGE 0 1)
                list(GET fog_flags ${fog_index} fog_flag)
                list(GET fog_ids ${fog_index} fog_id)

                _vulkan_shader_join_defines(defines "${tx_flag}" "${env_flag}" "${fog_flag}")
                _vulkan_shader_add_task(
                    "vert_${tx_id}${env_id}${fog_id}"
                    "gen_vert.tmpl" "vert" "${defines}"
                    "vk.modules.vert.gen[${tx_index}][0][${env_index}][${fog_index}]")

                if(NOT tx_index EQUAL 0)
                    _vulkan_shader_join_defines(defines "${tx_flag}" "${cl_flag}" "${env_flag}" "${fog_flag}")
                    _vulkan_shader_add_task(
                        "vert_${tx_id}_${cl_id}${env_id}${fog_id}"
                        "gen_vert.tmpl" "vert" "${defines}"
                        "vk.modules.vert.gen[${tx_index}][1][${env_index}][${fog_index}]")
                endif()
            endforeach()
        endforeach()
    endforeach()

    # Generic fragment shaders.
    foreach(tx_index RANGE 0 2)
        list(GET tx_flags ${tx_index} tx_flag)
        list(GET tx_ids ${tx_index} tx_id)
        list(GET cl_flags ${tx_index} cl_flag)
        list(GET cl_ids ${tx_index} cl_id)
        foreach(fog_index RANGE 0 1)
            list(GET fog_flags ${fog_index} fog_flag)
            list(GET fog_ids ${fog_index} fog_id)

            if(tx_index EQUAL 0)
                set(atest_flag "-DUSE_ATEST")
            else()
                set(atest_flag "")
            endif()

            _vulkan_shader_join_defines(defines "${tx_flag}" "${atest_flag}" "${fog_flag}")
            _vulkan_shader_add_task(
                "frag_${tx_id}${fog_id}"
                "gen_frag.tmpl" "frag" "${defines}"
                "vk.modules.frag.gen[${tx_index}][0][${fog_index}]")

            if(NOT tx_index EQUAL 0)
                _vulkan_shader_join_defines(defines "${tx_flag}" "${cl_flag}" "${fog_flag}")
                _vulkan_shader_add_task(
                    "frag_${tx_id}_${cl_id}${fog_id}"
                    "gen_frag.tmpl" "frag" "${defines}"
                    "vk.modules.frag.gen[${tx_index}][1][${fog_index}]")
            endif()
        endforeach()
    endforeach()
endfunction()

# Register the shader build step for target and make it consume the
# generated shader_data.c.
function(add_vulkan_shader_generation target)
    if(NOT USE_VULKAN_SHADER_COMPILE)
        message(STATUS "Vulkan shader data: generation disabled, using checked-in shader_data.c")
        return()
    endif()

    if(NOT VULKAN_GLSLANG_VALIDATOR)
        message(WARNING "Vulkan shader data: glslangValidator not found, using checked-in shader_data.c")
        return()
    endif()

    get_property(already_added GLOBAL PROPERTY VULKAN_SHADER_GENERATION_ADDED)
    if(already_added)
        return()
    endif()
    set_property(GLOBAL PROPERTY VULKAN_SHADER_GENERATION_ADDED TRUE)

    set(output_dir "${CMAKE_BINARY_DIR}/renderervk/shaders")
    set(spv_dir "${output_dir}/spv")
    file(MAKE_DIRECTORY "${spv_dir}")

    message(STATUS "Vulkan shader data: generating at build time with ${VULKAN_GLSLANG_VALIDATOR}")

    set_property(GLOBAL PROPERTY VULKAN_SHADER_TASKS "")
    _vulkan_shader_add_individual_variants()
    _vulkan_shader_add_template_variants()
    get_property(tasks GLOBAL PROPERTY VULKAN_SHADER_TASKS)

    set(spv_files "")
    foreach(task IN LISTS tasks)
        string(REPLACE "|" ";" fields "${task}")
        list(GET fields 0 name)
        list(GET fields 1 input)
        list(GET fields 2 stage)
        list(GET fields 3 defines)

        set(input_path "${VULKAN_SHADER_SOURCE_DIR}/${input}")
        set(spv "${spv_dir}/${name}.spv")

        set(define_args "")
        if(defines)
            separate_arguments(define_args UNIX_COMMAND "${defines}")
        endif()

        add_custom_command(
            OUTPUT "${spv}"
            COMMAND "${VULKAN_GLSLANG_VALIDATOR}"
                -s -S "${stage}" -V ${define_args} -o "${spv}" "${input_path}"
            DEPENDS "${input_path}"
            COMMENT "Compiling Vulkan shader ${name}"
            VERBATIM
        )

        list(APPEND spv_files "${spv}")
        set_source_files_properties("${spv}" PROPERTIES GENERATED TRUE)
    endforeach()

    set(data_file "${output_dir}/shader_data.c")
    set(binding_file "${output_dir}/shader_binding.c")

    set(generated_files "${data_file}")
    if(USE_VULKAN_SHADER_BINDINGS)
        list(APPEND generated_files "${binding_file}")
    endif()

    add_custom_command(
        OUTPUT ${generated_files}
        COMMAND "${CMAKE_COMMAND}"
            "-DVK_SHADER_OUTPUT_DIR=${output_dir}"
            "-DVK_SHADER_DATA_FILE=${data_file}"
            "-DVK_SHADER_BINDING_FILE=${binding_file}"
            "-DVK_SHADER_GENERATE_BINDINGS=${USE_VULKAN_SHADER_BINDINGS}"
            "-DVK_SHADER_TASKS=${tasks}"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/generate_vulkan_shader_data.cmake"
        DEPENDS ${spv_files}
        COMMENT "Generating Vulkan shader data"
        VERBATIM
    )

    set_source_files_properties(${generated_files}
        PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)

    target_sources(${target} PRIVATE ${generated_files})
    target_include_directories(${target} PRIVATE "${output_dir}")
    target_compile_definitions(${target} PRIVATE USE_GENERATED_VULKAN_SHADERS)
    set_source_files_properties("${SOURCE_DIR}/renderervk/vk.c"
        PROPERTIES OBJECT_DEPENDS "${data_file}")

    add_custom_target(vulkan_shader_data DEPENDS ${generated_files})
    set_target_properties(vulkan_shader_data PROPERTIES FOLDER Renderers)
    add_dependencies(${target} vulkan_shader_data)
endfunction()
