if(NOT BUILD_CLIENT)
    return()
endif()

set(INTERNAL_JPEG_DIR ${DEPS_DIR}/libjpeg)

if(USE_INTERNAL_JPEG)
    add_library(libjpeg STATIC 
        "${INTERNAL_JPEG_DIR}/jaricom.c"
        "${INTERNAL_JPEG_DIR}/jcapimin.c"
        "${INTERNAL_JPEG_DIR}/jcapistd.c"
        "${INTERNAL_JPEG_DIR}/jcarith.c"
        "${INTERNAL_JPEG_DIR}/jccoefct.c"
        "${INTERNAL_JPEG_DIR}/jccolor.c"
        "${INTERNAL_JPEG_DIR}/jcdctmgr.c"
        "${INTERNAL_JPEG_DIR}/jchuff.c"
        "${INTERNAL_JPEG_DIR}/jcinit.c"
        "${INTERNAL_JPEG_DIR}/jcmainct.c"
        "${INTERNAL_JPEG_DIR}/jcmarker.c"
        "${INTERNAL_JPEG_DIR}/jcmaster.c"
        "${INTERNAL_JPEG_DIR}/jcomapi.c"
        "${INTERNAL_JPEG_DIR}/jcparam.c"
        "${INTERNAL_JPEG_DIR}/jcprepct.c"
        "${INTERNAL_JPEG_DIR}/jcsample.c"
        "${INTERNAL_JPEG_DIR}/jctrans.c"
        "${INTERNAL_JPEG_DIR}/jdapimin.c"
        "${INTERNAL_JPEG_DIR}/jdapistd.c"
        "${INTERNAL_JPEG_DIR}/jdarith.c"
        "${INTERNAL_JPEG_DIR}/jdatadst.c"
        "${INTERNAL_JPEG_DIR}/jdatasrc.c"
        "${INTERNAL_JPEG_DIR}/jdcoefct.c"
        "${INTERNAL_JPEG_DIR}/jdcolor.c"
        "${INTERNAL_JPEG_DIR}/jddctmgr.c"
        "${INTERNAL_JPEG_DIR}/jdhuff.c"
        "${INTERNAL_JPEG_DIR}/jdinput.c"
        "${INTERNAL_JPEG_DIR}/jdmainct.c"
        "${INTERNAL_JPEG_DIR}/jdmarker.c"
        "${INTERNAL_JPEG_DIR}/jdmaster.c"
        "${INTERNAL_JPEG_DIR}/jdmerge.c"
        "${INTERNAL_JPEG_DIR}/jdpostct.c"
        "${INTERNAL_JPEG_DIR}/jdsample.c"
        "${INTERNAL_JPEG_DIR}/jdtrans.c"
        "${INTERNAL_JPEG_DIR}/jerror.c"
        "${INTERNAL_JPEG_DIR}/jfdctflt.c"
        "${INTERNAL_JPEG_DIR}/jfdctfst.c"
        "${INTERNAL_JPEG_DIR}/jfdctint.c"
        "${INTERNAL_JPEG_DIR}/jidctflt.c"
        "${INTERNAL_JPEG_DIR}/jidctfst.c"
        "${INTERNAL_JPEG_DIR}/jidctint.c"
        "${INTERNAL_JPEG_DIR}/jmemmgr.c"
        "${INTERNAL_JPEG_DIR}/jmemnobs.c"
        "${INTERNAL_JPEG_DIR}/jquant1.c"
        "${INTERNAL_JPEG_DIR}/jquant2.c"
        "${INTERNAL_JPEG_DIR}/jutils.c"  
    )

    target_compile_definitions(libjpeg PRIVATE _LIB)
    target_include_directories(libjpeg PRIVATE ${INTERNAL_JPEG_DIR})
    target_compile_definitions(libjpeg PRIVATE $<$<C_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>)
    target_link_libraries(libjpeg PRIVATE c_compiler_opts_w0)
    list(APPEND CLIENT_LIBRARIES libjpeg)
    list(APPEND CLIENT_DEFINITIONS USE_INTERNAL_JPEG)
    list(APPEND CLIENT_INCLUDE_DIRS ${INTERNAL_JPEG_DIR})
    set_target_properties(libjpeg PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER} PREFIX "")
else()
    find_package(JPEG REQUIRED)
    list(APPEND CLIENT_LIBRARIES JPEG::JPEG)
endif()
