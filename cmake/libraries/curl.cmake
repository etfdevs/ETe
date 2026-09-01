# curl is mandatory for ET Client support

if(NOT BUILD_CLIENT)
    return()
endif()

if(WIN32)
    # libcurl setup
    set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "") # Forces static
    set(CURL_STATICLIB ON CACHE INTERNAL "")
    set(BUILD_LIBCURL_DOCS OFF CACHE INTERNAL "")
    set(BUILD_MISC_DOCS OFF CACHE INTERNAL "")
    set(ENABLE_CURL_MANUAL OFF CACHE INTERNAL "")
    set(BUILD_TESTING OFF CACHE INTERNAL "")
    set(BUILD_EXAMPLES OFF CACHE INTERNAL "")
    set(CURL_CA_NATIVE ON CACHE INTERNAL "")
    set(PERL_EXECUTABLE "" CACHE INTERNAL "")
    set(CURL_STATIC_CRT ON CACHE INTERNAL "")
    # Disable modern Windows 10+ specific features if needed
    set(USE_WIN32_IDN OFF CACHE INTERNAL "") # No International Domain Names
    set(USE_LIBIDN2 OFF CACHE INTERNAL "")
    set(CURL_DISABLE_INSTALL ON CACHE INTERNAL "")
    # Make sure these are for http and ftp support
    set(HTTP_ONLY OFF CACHE INTERNAL "")
    set(CURL_DISABLE_FTP OFF CACHE INTERNAL "")
    set(CURL_DISABLE_HTTP OFF CACHE INTERNAL "")

    # The "Everything Else" Disable List
    foreach(feature 
        ALTSVC COOKIES DICT DOH FILE GOPHER HSTS IMAP LDAP LDAPS
        MIME MQTT MQTTS NTLM POP3 PROXY RTSP SMB SMTP SOCKETPAIR
        TELNET TFTP WEBSOCKETS HTTP_AUTH IPFS)
        set(CURL_DISABLE_${feature} ON CACHE INTERNAL "")
    endforeach()
    set(CURL_USE_SCHANNEL ON CACHE INTERNAL "")
    set(USE_NGHTTP2 OFF CACHE INTERNAL "")
    set(CURL_USE_LIBPSL OFF CACHE INTERNAL "")
    set(CURL_USE_LIBSSH2 OFF CACHE INTERNAL "")
    set(CURL_BROTLI OFF CACHE INTERNAL "")
    set(CURL_ZLIB OFF CACHE INTERNAL "")
    set(CURL_ZSTD OFF CACHE INTERNAL "")
    set(ENABLE_UNIX_SOCKETS OFF CACHE INTERNAL "")
    set(CURL_HIDDEN_SYMBOLS ON CACHE INTERNAL "")    # Better for static linking optimization

    include(FetchContent)
    FetchContent_Declare(curl
        URL https://curl.se/download/curl-8.21.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP true
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(curl)
    find_package(curl)
    list(APPEND CLIENT_DEFINITIONS USE_CURL CURL_STATICLIB)
    list(APPEND CLIENT_LIBRARIES CURL::libcurl)
    list(APPEND CLIENT_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
    set_target_properties(libcurl_object libcurl_static PROPERTIES FOLDER ${BUNDLED_TARGETS_FOLDER})
else()
    find_package(CURL REQUIRED)
    list(APPEND CLIENT_LIBRARIES CURL::libcurl)
    list(APPEND CLIENT_DEFINITIONS USE_CURL USE_CURL_DLOPEN)

    if(NOT CURL_FOUND)
        message(FATAL_ERROR "CURL support required for ETe")
    endif()
endif()
