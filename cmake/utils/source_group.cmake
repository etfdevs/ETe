# THIS CODE IS TAKEN FROM THE ETLEGACY PROJECT WITH SLIGHT EDITS

#-----------------------------------------------------------------
# Source Group (for Visual Studio and win32 definitions)
#-----------------------------------------------------------------
include_guard(GLOBAL)

if(MSVC OR XCODE)
	# Group the files based on their source path
	set(SRC_PATH "src")
	file(GLOB_RECURSE ALL_SOURCES "${SRC_PATH}/*.*")
	get_filename_component(SRC_FULLPATH ${SRC_PATH} ABSOLUTE)

	foreach (SRCFILE ${ALL_SOURCES})
		get_filename_component(FILE_FOLDER ${SRCFILE} PATH)
		get_filename_component(FILE_EXT ${SRCFILE} EXT)

		string(REPLACE "${SRC_FULLPATH}/" "" FILE_FOLDER "${FILE_FOLDER}")
		string(REPLACE "${SRC_FULLPATH}" "" FILE_FOLDER "${FILE_FOLDER}")

		if(FILE_EXT STREQUAL ".cpp" OR FILE_EXT STREQUAL ".c" OR FILE_EXT STREQUAL ".cc" OR FILE_EXT STREQUAL ".cxx")
			source_group("Source Files/${FILE_FOLDER}" FILES ${SRCFILE})
		elseif(FILE_EXT STREQUAL ".asm" OR FILE_EXT STREQUAL ".s")
			source_group("Assembly Files" FILES ${SRCFILE})
		elseif(FILE_EXT STREQUAL ".frag" OR FILE_EXT STREQUAL ".vert" OR FILE_EXT STREQUAL ".tmpl")
			source_group("Shader Files" FILES ${SRCFILE})
		elseif(FILE_EXT STREQUAL ".hpp" OR FILE_EXT STREQUAL ".h")
			source_group("Header Files/${FILE_FOLDER}" FILES ${SRCFILE})
		elseif(FILE_EXT STREQUAL ".rc")
			source_group("Resource Files" FILES ${SRCFILE})
		endif()
	endforeach()

	#set(ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets")
	#file(GLOB_RECURSE ASSETS "${ASSETS_PATH}/*.*")	
	#list(REMOVE_ITEM ASSETS "${ASSETS_PATH}/CMakeLists.txt")

	# dummy target under which the assets are places
	#add_custom_target(assets SOURCES ${ASSETS})
	#source_group(TREE "${ASSETS_PATH}" FILES ${ASSETS})
endif()
