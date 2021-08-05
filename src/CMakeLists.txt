cmake_minimum_required(VERSION 3.6)
project(ETe C CXX)

# print compiler flags
# set( CMAKE_VERBOSE_MAKEFILE on )

# Set a default build type if none was specified
set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "RelWithDebInfo")
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    # Ensure debug symbols in backtrace
    link_libraries("-rdynamic")
endif()

Include(CheckTypeSize)
check_type_size("void*" CMAKE_SIZEOF_VOID_P)

#make all this cleaner and match rest of cmake I guess?
#if(WIN32)
#	set(LIB_SUFFIX "_mp_")
#	set(X86 ON)
#	if(CMAKE_SIZEOF_VOID_P MATCHES "8")
#		set(Architecture "x86_64")
#		set(WIN64 TRUE)
#	else()
#		set(Architecture "x86")
#		set(WIN64 FALSE)
#	endif()
#else()
	if(APPLE)
		set(LIB_SUFFIX "_mac")
	else()
		set(LIB_SUFFIX ".mp.")
	endif()

    if(APPLE)
        set(DLL_EXT ".dylib")
    elseif(NOT WIN32)
        set(DLL_EXT ".so")
    else()
        set(DLL_EXT ".dll")
    endif()

    set(X86 OFF)
    set(X86_64 OFF)
    set(ARM32 OFF)
    set(ARM64 OFF)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(ARM32 ON)
        set(Architecture "arm")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
        set(ARM64 ON)
        set(Architecture "aarch64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
		set(X86 ON)
#		if(APPLE)
#			set(Architecture "x86")
#		else()
			# e.g. Linux
			set(Architecture "i386")
#		endif()
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^x86.64$")
        #set(X86 ON)
        set(X86_64 ON)
		set(Architecture "x86_64")
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "powerpc")
		set(Architecture "ppc")
	elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "powerpc64")
		set(Architecture "ppc64")
	else()
		set(Architecture "${CMAKE_SYSTEM_PROCESSOR}")
	endif()
#endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    link_libraries("-Wl,--hash-style=both")
endif()

message("Configured for ${CMAKE_SYSTEM_PROCESSOR}")

option(BUILD_CLIENT "Build the ete client binary" ON)
option(BUILD_DEDSERVER "Build the ete dedicated server binary" ON)
option(USE_SDL2 "Whether to use native backends or SDL2" ON)
option(USE_SYSTEM_JPEG "Look for and use system jpeg library instead of locally built" ON)
option(DYNAMIC_RENDERER "Whether to use modular rendering features (builds as shared object instead of library" OFF)
#option(USE_DISCORD "Use discord rich presence API" OFF)
option(BUILD_ETMAIN_MOD "Build cgame/qagame/ui modules for etmain" OFF)
option(ENABLE_SPLINES "Splines code" ON)

set(USE_DISCORD OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
set(compiler_flags_global -pipe -Wall -fmessage-length=0 -fno-strict-aliasing -fvisibility=hidden -fcolor-diagnostics)
else()
set(compiler_flags_global -pipe -Wall -fmessage-length=0 -fno-strict-aliasing -fvisibility=hidden -fdiagnostics-color=always)
endif()
# debug: explicit -Og -D_DEBUG, implicit -g
set(compiler_flags_debug ${compiler_flags_global} -Og -D_DEBUG)
# relwithdebinfo: implicit -O2 -g -DNDEBUG
set(compiler_flags_relwithdebinfo ${compiler_flags_global})
# release: implicit -O3 -DNDEBUG
if(APPLE AND X86_64)
set(compiler_flags_release ${compiler_flags_global})
elseif(X86_64)
set(compiler_flags_release ${compiler_flags_global})
else()
set(compiler_flags_release ${compiler_flags_global} -march=i686 -Winline -ffast-math -fomit-frame-pointer -finline-functions -fschedule-insns2)
endif()

set(SRCDIR "${CMAKE_SOURCE_DIR}")

set(botlib_files 
    "botlib/be_aas_bspq3.c"
    "botlib/be_aas_cluster.c"
    "botlib/be_aas_debug.c"
    "botlib/be_aas_entity.c"
    "botlib/be_aas_file.c"
    "botlib/be_aas_main.c"
    "botlib/be_aas_move.c"
    "botlib/be_aas_optimize.c"
    "botlib/be_aas_reach.c"
    "botlib/be_aas_route.c"
    "botlib/be_aas_routealt.c"
    "botlib/be_aas_routetable.c"
    "botlib/be_aas_sample.c"
    "botlib/be_ai_char.c"
    "botlib/be_ai_chat.c"
    "botlib/be_ai_gen.c"
    "botlib/be_ai_goal.c"
    "botlib/be_ai_move.c"
    "botlib/be_ai_weap.c"
    "botlib/be_ai_weight.c"
    "botlib/be_ea.c"
    "botlib/be_interface.c"
    "botlib/l_crc.c"
    "botlib/l_libvar.c"
    "botlib/l_log.c"
    "botlib/l_memory.c"
    "botlib/l_precomp.c"
    "botlib/l_script.c"
    "botlib/l_struct.c"
)

set(client_files
    "client/cl_avi.c"
    "client/cl_cgame.c"
    "client/cl_cin.c"
    "client/cl_console.c"
    "client/cl_curl.c"
    "client/cl_discord.c"
    "client/cl_input.c"
    "client/cl_jpeg.c"
    "client/cl_keys.c"
    "client/cl_main.c"
    "client/cl_net_chan.c"
    "client/cl_parse.c"
    "client/cl_scrn.c"
    "client/cl_ui.c"
    "client/snd_adpcm.c"
    "client/snd_codec_wav.c"
    "client/snd_codec.c"
    "client/snd_dma.c"
    "client/snd_main.c"
    "client/snd_mem.c"
    "client/snd_mix.c"
    "client/snd_wavelet.c"
)

set(server_files
    "server/sv_bot.c"
    "server/sv_ccmds.c"
    "server/sv_client.c"
    "server/sv_filter.c"
    "server/sv_game.c"
    "server/sv_init.c"
    "server/sv_main.c"
    "server/sv_net_chan.c"
    "server/sv_snapshot.c"
    "server/sv_world.c"
)

set(qcommon_files
    "qcommon/cm_load.c"
    "qcommon/cm_patch.c"
    "qcommon/cm_polylib.c"
    "qcommon/cm_test.c"
    "qcommon/cm_trace.c"
    "qcommon/cmd.c"
    "qcommon/common.c"
    "qcommon/cvar.c"
    "qcommon/files.c"
    "qcommon/gameinfo.c"
    "qcommon/history.c"
    "qcommon/huffman_static.c"
    "qcommon/huffman.c"
    "qcommon/keys.c"
    "qcommon/md4.c"
    "qcommon/md5.c"
    "qcommon/msg.c"
    "qcommon/net_chan.c"
    "qcommon/net_ip.c"
    "qcommon/puff.c"
    "qcommon/q_math.c"
    "qcommon/q_shared.c"
    "qcommon/unzip.c"
    "qcommon/vm.c"
)

set(splines_files
    "splines/math_angles.cpp"
    "splines/math_matrix.cpp"
    "splines/math_quaternion.cpp"
    "splines/math_vector.cpp"
    "splines/q_parse.cpp"
    "splines/q_shared.c"
    "splines/splines.cpp"
    "splines/util_str.cpp"
)

set(linux_shared_files
    "unix/linux_signals.c"
    "unix/unix_main.c"
    "unix/unix_shared.c"
)

set(linux_client_files
    "unix/linux_glimp.c"
    "unix/linux_qgl.c"
    #"unix/linux_qvk.c"
    "unix/linux_snd.c"
    "unix/x11_dga.c"
    "unix/x11_randr.c"
    "unix/x11_vidmode.c"
)

if(USE_SDL2)
    set(sdl_files
        "sdl/sdl_gamma.c"
        "sdl/sdl_glimp.c"
        "sdl/sdl_glw.h"
        "sdl/sdl_icon.h"
        "sdl/sdl_input.c"
        "sdl/sdl_snd.c"
        "sdl/sdl_version.c"
    )
endif(USE_SDL2)

if(NOT USE_SYSTEM_JPEG)
    set(jpeg_local_files
        "jpeg-8c/jaricom.c"
        "jpeg-8c/jcapimin.c"
        "jpeg-8c/jcapistd.c"
        "jpeg-8c/jcarith.c"
        "jpeg-8c/jccoefct.c"
        "jpeg-8c/jccolor.c"
        "jpeg-8c/jcdctmgr.c"
        "jpeg-8c/jchuff.c"
        "jpeg-8c/jcinit.c"
        "jpeg-8c/jcmainct.c"
        "jpeg-8c/jcmarker.c"
        "jpeg-8c/jcmaster.c"
        "jpeg-8c/jcomapi.c"
        "jpeg-8c/jcparam.c"
        "jpeg-8c/jcprepct.c"
        "jpeg-8c/jcsample.c"
        "jpeg-8c/jctrans.c"
        "jpeg-8c/jdapimin.c"
        "jpeg-8c/jdapistd.c"
        "jpeg-8c/jdarith.c"
        "jpeg-8c/jdatadst.c"
        "jpeg-8c/jdatasrc.c"
        "jpeg-8c/jdcoefct.c"
        "jpeg-8c/jdcolor.c"
        "jpeg-8c/jddctmgr.c"
        "jpeg-8c/jdhuff.c"
        "jpeg-8c/jdinput.c"
        "jpeg-8c/jdmainct.c"
        "jpeg-8c/jdmarker.c"
        "jpeg-8c/jdmaster.c"
        "jpeg-8c/jdmerge.c"
        "jpeg-8c/jdpostct.c"
        "jpeg-8c/jdsample.c"
        "jpeg-8c/jdtrans.c"
        "jpeg-8c/jerror.c"
        "jpeg-8c/jfdctflt.c"
        "jpeg-8c/jfdctfst.c"
        "jpeg-8c/jfdctint.c"
        "jpeg-8c/jidctflt.c"
        "jpeg-8c/jidctfst.c"
        "jpeg-8c/jidctint.c"
        "jpeg-8c/jmemmgr.c"
        "jpeg-8c/jmemnobs.c"
        "jpeg-8c/jquant1.c"
        "jpeg-8c/jquant2.c"
        "jpeg-8c/jutils.c"
    )
endif(NOT USE_SYSTEM_JPEG)

#note renderer needs extra files in modular mode
set(renderer_files
    "renderer/tr_animation_mdm.c"
    "renderer/tr_animation_mds.c"
    "renderer/tr_arb.c"
    "renderer/tr_backend.c"
    "renderer/tr_bsp.c"
    "renderer/tr_cmds.c"
    "renderer/tr_cmesh.c"
    "renderer/tr_curve.c"
    "renderer/tr_decals.c"
    "renderer/tr_flares.c"
    "renderercommon/tr_font.c"
    "renderer/tr_image.c"
    "renderercommon/tr_image_bmp.c"
    "renderercommon/tr_image_jpg.c"
    "renderercommon/tr_image_pcx.c"
    "renderercommon/tr_image_png.c"
    "renderercommon/tr_image_tga.c"
    "renderer/tr_init.c"
    "renderer/tr_light.c"
    "renderer/tr_main.c"
    "renderer/tr_marks.c"
    "renderer/tr_mesh.c"
    "renderer/tr_model_iqm.c"
    "renderer/tr_model.c"
    "renderercommon/tr_noise.c"
    "renderer/tr_scene.c"
    "renderer/tr_shade_calc.c"
    "renderer/tr_shade.c"
    "renderer/tr_shader.c"
    "renderer/tr_shadows.c"
    "renderer/tr_sky.c"
    "renderer/tr_surface.c"
    "renderer/tr_vbo.c"
    "renderer/tr_world.c"
)

if(DYNAMIC_RENDERER)
    set(renderer_files
        ${renderer_files}
        "qcommon/q_shared.c"
        "qcommon/puff.c"
        "qcommon/q_math.c"
    )
endif(DYNAMIC_RENDERER)

if(BUILD_ETMAIN_MOD)
    set(cgame_files
        "cgame/cg_atmospheric.c"
        "cgame/cg_character.c"
        "cgame/cg_commandmap.c"
        "cgame/cg_consolecmds.c"
        "cgame/cg_debriefing.c"
        "cgame/cg_draw.c"
        "cgame/cg_drawtools.c"
        "cgame/cg_effects.c"
        "cgame/cg_ents.c"
        "cgame/cg_event.c"
        "cgame/cg_fireteamoverlay.c"
        "cgame/cg_fireteams.c"
        "cgame/cg_flamethrower.c"
        "cgame/cg_info.c"
        "cgame/cg_limbopanel.c"
        "cgame/cg_loadpanel.c"
        "cgame/cg_localents.c"
        "cgame/cg_main.c"
        "cgame/cg_marks.c"
        "cgame/cg_missionbriefing.c"
        "cgame/cg_multiview.c"
        "cgame/cg_newDraw.c"
        "cgame/cg_particles.c"
        "cgame/cg_players.c"
        "cgame/cg_playerstate.c"
        "cgame/cg_polybus.c"
        "cgame/cg_popupmessages.c"
        "cgame/cg_predict.c"
        "cgame/cg_scoreboard.c"
        "cgame/cg_servercmds.c"
        "cgame/cg_snapshot.c"
        "cgame/cg_sound.c"
        "cgame/cg_spawn.c"
        "cgame/cg_statsranksmedals.c"
        "cgame/cg_syscalls.c"
        "cgame/cg_trails.c"
        "cgame/cg_view.c"
        "cgame/cg_weapons.c"
        "cgame/cg_window.c"
        "game/bg_animation.c"
        "game/bg_animgroup.c"
        "game/bg_character.c"
        "game/bg_classes.c"
        "game/bg_misc.c"
        "game/bg_pmove.c"
        "game/bg_slidemove.c"
        "game/bg_sscript.c"
        "game/bg_stats.c"
        "game/bg_tracemap.c"
        "qcommon/q_math.c"
        "qcommon/q_shared.c"
        "ui/ui_shared.c"
    )

    set(qagame_files
        "game/bg_animation.c"
        "game/bg_animgroup.c"
        "game/bg_campaign.c"
        "game/bg_character.c"
        "game/bg_classes.c"
        "game/bg_misc.c"
        "game/bg_pmove.c"
        "game/bg_slidemove.c"
        "game/bg_sscript.c"
        "game/bg_stats.c"
        "game/bg_tracemap.c"
        "game/g_active.c"
        "game/g_alarm.c"
        "game/g_antilag.c"
        "game/g_buddy_list.c"
        "game/g_character.c"
        "game/g_client.c"
        "game/g_cmds.c"
        "game/g_cmds_ext.c"
        "game/g_combat.c"
        "game/g_config.c"
        "game/g_fireteams.c"
        "game/g_items.c"
        "game/g_main.c"
        "game/g_match.c"
        "game/g_mem.c"
        "game/g_misc.c"
        "game/g_missile.c"
        "game/g_mover.c"
        "game/g_multiview.c"
        "game/g_props.c"
        "game/g_referee.c"
        "game/g_script.c"
        "game/g_script_actions.c"
        "game/g_session.c"
        "game/g_spawn.c"
        "game/g_stats.c"
        "game/g_svcmds.c"
        "game/g_syscalls.c"
        "game/g_systemmsg.c"
        "game/g_target.c"
        "game/g_team.c"
        "game/g_teammapdata.c"
        "game/g_trigger.c"
        "game/g_utils.c"
        "game/g_vote.c"
        "game/g_weapon.c"
        "qcommon/q_math.c"
        "qcommon/q_shared.c"
    )

    set(ui_files
        "ui/ui_atoms.c"
        "ui/ui_gameinfo.c"
        "ui/ui_loadpanel.c"
        "ui/ui_main.c"
        "ui/ui_shared.c"
        "ui/ui_syscalls.c"
        "game/bg_campaign.c"
        "game/bg_classes.c"
        "game/bg_misc.c"
        "qcommon/q_math.c"
        "qcommon/q_shared.c"
    )
endif(BUILD_ETMAIN_MOD)

add_library(botlib STATIC "${botlib_files}")
target_compile_options(botlib
    PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
            $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release} -w>
            $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release} -w>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
)
target_include_directories(botlib PUBLIC "${SRCDIR}/botlib ${SRCDIR}/qcommon")
target_compile_definitions(botlib PUBLIC "BOTLIB")

if(ENABLE_SPLINES)
add_library(splines STATIC "${splines_files}")
target_compile_options(splines
    PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
            $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release} -w>
            $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release} -w>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
)
target_include_directories(splines PUBLIC "${SRCDIR}/splines")
#target_link_libraries(splines PRIVATE "libstdc++")
endif(ENABLE_SPLINES)

set(RENDERER_PREFIX "ete")
if(BUILD_CLIENT)
    if(DYNAMIC_RENDERER)
        add_library(${RENDERER_PREFIX}_opengl SHARED "${renderer_files}")
    else()
        add_library(${RENDERER_PREFIX}_opengl STATIC "${renderer_files}")
    endif(DYNAMIC_RENDERER)

    target_compile_options(${RENDERER_PREFIX}_opengl
        PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo}>
    )

    set_target_properties(${RENDERER_PREFIX}_opengl PROPERTIES PREFIX "")

    if(DYNAMIC_RENDERER)
        target_compile_definitions(${RENDERER_PREFIX}_opengl PUBLIC "USE_RENDERER_DLOPEN" "RENDERER_PREFIX=\"${RENDERER_PREFIX}\"")
        set_target_properties(${RENDERER_PREFIX}_opengl PROPERTIES SUFFIX "_${Architecture}${DLL_EXT}")
        target_link_libraries(${RENDERER_PREFIX}_opengl PRIVATE ${CMAKE_DL_LIBS} "m")
    endif(DYNAMIC_RENDERER)

    target_include_directories(${RENDERER_PREFIX}_opengl PUBLIC "${SRCDIR}/renderer ${SRCDIR}/renderercommon ${SRCDIR}/qcommon")
endif(BUILD_CLIENT)

set(core_client_files
    "${client_files}"
    "${qcommon_files}"
    "${server_files}"
    "${linux_shared_files}"
)

if(USE_SDL2)
    set(core_client_files
        "${core_client_files}"
        "${sdl_files}"
    )
else()
    set(core_client_files
        "${core_client_files}"
        "${linux_client_files}"
    )
endif(USE_SDL2)

if(NOT USE_SYSTEM_JPEG)
    set(core_client_files
        "${core_client_files}"
        "${jpeg_local_files}"
    )
endif(NOT USE_SYSTEM_JPEG)

set(dedicated_server_files
    "${qcommon_files}"
    "${server_files}"
    "${linux_shared_files}"
)

if(BUILD_CLIENT)
    #find_package(OpenGL REQUIRED) detected at runtime
    if(USE_SYSTEM_JPEG)
        find_package(JPEG REQUIRED)
    endif(USE_SYSTEM_JPEG)
    find_package(CURL REQUIRED)
    if(USE_SDL2)
        find_package(SDL2 REQUIRED CONFIG)

        if (TARGET SDL2::SDL2)
            set(SDL2_INCLUDE_DIRS "")
            set(SDL2_LIBRARIES SDL2::SDL2)
        endif()
    endif(USE_SDL2)

    add_executable(ete "${core_client_files}")
    target_compile_options(ete
        PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo}>
    )
    if(X86)
        set_target_properties(ete PROPERTIES SUFFIX ".x86")
    else()
        set_target_properties(ete PROPERTIES SUFFIX ".${Architecture}") #NOTE changeme when multi-platform and multi-arch support is added
    endif(X86)
    target_include_directories(ete PUBLIC "${SRCDIR}/botlib ${SRCDIR}/server ${SRCDIR}/client ${SRCDIR}/qcommon")
    target_compile_definitions(ete PUBLIC "USE_CURL" "USE_CURL_DLOPEN")
    target_include_directories(ete PRIVATE ${CURL_INCLUDE_DIRS})
    if(APPLE)
    target_link_libraries(ete PRIVATE ${CMAKE_DL_LIBS} m pthread ${CURL_LIBRARIES})
    else()
    target_link_libraries(ete PRIVATE ${CMAKE_DL_LIBS} m pthread X11 ${CURL_LIBRARIES})
    endif()
    if(DYNAMIC_RENDERER)
        target_compile_definitions(ete PUBLIC "USE_RENDERER_DLOPEN" "RENDERER_PREFIX=\"${RENDERER_PREFIX}\"")
    endif(DYNAMIC_RENDERER)
    if(USE_SDL2)
        target_compile_definitions(ete PUBLIC "USE_SDL" "USE_ICON")
        target_include_directories(ete PRIVATE ${SDL2_INCLUDE_DIRS})
        string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
        target_link_libraries(ete PRIVATE ${SDL2_LIBRARIES})
    endif(USE_SDL2)
    #if(USE_DISCORD)
    #    target_compile_definitions(ete PUBLIC "USE_DISCORD")
    #endif(USE_SDL2)
    if(USE_SYSTEM_JPEG)
        target_compile_definitions(ete PUBLIC "USE_SYSTEM_JPEG")
        target_include_directories(ete PRIVATE ${JPEG_INCLUDE_DIRS})
        target_link_libraries(ete PRIVATE ${JPEG_LIBRARIES})
    endif()
    target_link_libraries(ete PUBLIC botlib)
    if(ENABLE_SPLINES)
    target_compile_definitions(ete PUBLIC "ENABLE_SPLINES")
        target_link_libraries(ete PUBLIC splines)
    endif()
    if(NOT DYNAMIC_RENDERER)
        target_link_libraries(ete PUBLIC ${RENDERER_PREFIX}_opengl)
    endif(NOT DYNAMIC_RENDERER)
endif(BUILD_CLIENT)

if(BUILD_DEDSERVER)
    add_executable(ete-ded "${dedicated_server_files}")
    target_compile_options(ete-ded
        PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo}>
    )
    if(X86)
        set_target_properties(ete-ded PROPERTIES SUFFIX ".x86")
    else()
        set_target_properties(ete-ded PROPERTIES SUFFIX ".${Architecture}")
    endif(X86)
    target_include_directories(ete-ded PUBLIC "${SRCDIR}/botlib ${SRCDIR}/server ${SRCDIR}/client ${SRCDIR}/qcommon")
    target_compile_definitions(ete-ded PUBLIC "DEDICATED")
    target_link_libraries(ete-ded PRIVATE ${CMAKE_DL_LIBS} "m")
    target_link_libraries(ete-ded PUBLIC botlib)
endif(BUILD_DEDSERVER)

if(BUILD_ETMAIN_MOD)
    add_library(cgame SHARED "${cgame_files}")
    target_compile_options(cgame
        PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
    )
    set_target_properties(cgame PROPERTIES PREFIX "")
    set_target_properties(cgame PROPERTIES SUFFIX "${LIB_SUFFIX}${Architecture}${DLL_EXT}") #NOTE changeme when multi-platform and multi-arch support is added
    target_compile_definitions(cgame PUBLIC "CGAMEDLL")
    target_link_libraries(cgame PRIVATE ${CMAKE_DL_LIBS} "m")

    add_library(qagame SHARED "${qagame_files}")
    target_compile_options(qagame
        PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
    )
    set_target_properties(qagame PROPERTIES PREFIX "")
    set_target_properties(qagame PROPERTIES SUFFIX "${LIB_SUFFIX}${Architecture}${DLL_EXT}") #NOTE changeme when multi-platform and multi-arch support is added
    target_compile_definitions(qagame PUBLIC "GAMEDLL" "NO_BOT_SUPPORT")
    target_link_libraries(qagame PRIVATE ${CMAKE_DL_LIBS} "m")

    add_library(ui SHARED "${ui_files}")
    target_compile_options(ui
        PRIVATE $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:DEBUG>>:${compiler_flags_debug} -w>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELEASE>>:${compiler_flags_release}>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:RELWITHDEBINFO>>:${compiler_flags_relwithdebinfo} -w>
    )
    set_target_properties(ui PROPERTIES PREFIX "")
    set_target_properties(ui PROPERTIES SUFFIX "${LIB_SUFFIX}${Architecture}${DLL_EXT}") #NOTE changeme when multi-platform and multi-arch support is added
    target_compile_definitions(ui PUBLIC "UIDLL")
    target_link_libraries(ui PRIVATE ${CMAKE_DL_LIBS} "m")

    #add_custom_target(create_etmain_pak ALL
    #    COMMAND ${CMAKE_COMMAND} -E tar "cfv" "mp_bina.pk3" --format=zip $<TARGET_FILE:cgame> $<TARGET_FILE:ui>
    #    DEPENDS cgame ui
    #)

endif(BUILD_ETMAIN_MOD)

if(BUILD_DEDSERVER)
    install(TARGETS ete-ded
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
    )
endif(BUILD_DEDSERVER)

if(BUILD_CLIENT)
    install(TARGETS ete
            RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}
)
endif(BUILD_CLIENT)

if(DYNAMIC_RENDERER)
    install(TARGETS ${RENDERER_PREFIX}_opengl
            LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}
    )
endif(DYNAMIC_RENDERER)

if(BUILD_ETMAIN_MOD)
    install(TARGETS cgame qagame ui
            LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/etmain"
    )
    #install(FILES "${CMAKE_BINARY_DIR}/mp_bina.pk3"
    #        DESTINATION "${CMAKE_INSTALL_PREFIX}/etmain"
    #)
endif(BUILD_ETMAIN_MOD)
