cmake_minimum_required(VERSION 3.16)

file (STRINGS "version" BUILD_VERSION)

project(rithmic_zorro_plugin
        VERSION ${BUILD_VERSION}
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG)
endif()

find_package(spdlog REQUIRED CONFIG)
add_definitions(-DSPDLOG_USE_STD_FORMAT -DSPDLOG_USE_STD_FORMAT_HO)

find_package(date CONFIG REQUIRED)

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)
configure_file(resources/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

# Enable precompiled headers
set(CMAKE_CXX_PCH_ENABLED TRUE)

set(RITHMIC_INCLUDE_DIR "$ENV{RITHMIC_API_INCLUDE_DIR}")
set(RITHMIC_LIB_DIR "$ENV{RITHMIC_API_LIB_DIR}")

message(STATUS "R|Api+ include: ${RITHMIC_INCLUDE_DIR}")
message(STATUS "R|Api+ lib: ${RITHMIC_LIB_DIR}")
message(STATUS "Zorro: $ENV{ZORRO_ROOT}")
message(STATUS "spdlog: ${spdlog_DIR}")
message(STATUS "date: ${date_DIR}")

if (NOT EXISTS ${RITHMIC_INCLUDE_DIR})
    message(FATAL_ERROR "R|Api+ include diretory ${RITHMIC_INCLUDE_DIR} doesn't exist")
endif()

if (NOT EXISTS ${RITHMIC_LIB_DIR})
    message(FATAL_ERROR "R|Api+ lib diretory ${RITHMIC_LIB_DIR} doesn't exist")
endif()

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

# Define resource files
set(RESOURCES
    resources/resource.rc
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

add_library(rithmic_zorro_plugin SHARED ${SOURCE_FILES} ${RESOURCES})

set_target_properties(rithmic_zorro_plugin PROPERTIES OUTPUT_NAME "Rithmic")

target_include_directories(rithmic_zorro_plugin PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${RITHMIC_INCLUDE_DIR}
    3rdparty
    3rdparty/zorro
    resources
    src
)

add_definitions(-D_WIN32_WINNT=0x0A00)
set(CMAKE_SUPPRESS_REGENERATION true)   # supress zero_check
set_target_properties(rithmic_zorro_plugin PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if (CMAKE_BUILD_TYPE MATCHES Debug)
    if (DEFINED ENV{ZORRO_ROOT})
        if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "win32")
            set_target_properties(rithmic_zorro_plugin PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "$ENV{ZORRO_ROOT}/plugin")
        else()
            set_target_properties(rithmic_zorro_plugin PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "$ENV{ZORRO_ROOT}/plugin64")
        endif()
    else()
        set_target_properties(rithmic_zorro_plugin PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug")
    endif()
else()
    set_target_properties(rithmic_zorro_plugin PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release")
endif()
set_target_properties(rithmic_zorro_plugin PROPERTIES LINK_INCREMENTAL ON)
target_compile_definitions(rithmic_zorro_plugin PUBLIC _UNICODE) # set CharacterSet to unicode
target_compile_definitions(rithmic_zorro_plugin PUBLIC WIN32 _WINDOWS _USRDLL Rithmic_EXPORTS) # set CharacterSet to unicode
# target_compile_options(rithmic_zorro_plugin PRIVATE "/bigobj")
# target_compile_options(rithmic_zorro_plugin PRIVATE /wd4312)
set_target_properties(rithmic_zorro_plugin PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_precompile_headers(rithmic_zorro_plugin PRIVATE "src/stdafx.h")

target_link_directories(rithmic_zorro_plugin PRIVATE ${RITHMIC_LIB_DIR})

if (CMAKE_BUILD_TYPE MATCHES Debug)
    if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "win32")
        target_link_libraries(rithmic_zorro_plugin PRIVATE
            RApiPlus_mtd.lib
            OmneStreamEngine_mtd.lib
            OmneChannel_mtd.lib
            OmneEngine_mtd.lib
            api_mtd.lib
            apistb_mtd.lib
            kit_mtd.lib
            libcrypto_mtd.lib
            libssl_mtd.lib
            zlib_mtd.lib
            libucrtd.lib
            libvcruntimed.lib
            libcmtd.lib
            LIBCPMTD.LIB
        )
    else()
        target_link_libraries(rithmic_zorro_plugin PRIVATE
            RApiPlus_mtd64.lib
            OmneStreamEngine_mtd64.lib
            OmneChannel_mtd64.lib
            OmneEngine_mtd64.lib
            api_mtd64.lib
            apistb_mtd64.lib
            kit_mtd64.lib
            libcrypto_mtd64.lib
            libssl_mtd64.lib
            zlib_mtd64.lib
            libucrtd.lib
            libvcruntimed.lib
            libcmtd.lib
            LIBCPMTD.LIB
        )
    endif()
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "win32")
        target_link_libraries(rithmic_zorro_plugin PRIVATE
            RApiPlus_mt.lib
            OmneStreamEngine_mt.lib
            OmneChannel_mt.lib
            OmneEngine_mt.lib
            api_mt.lib
            apistb_mt.lib
            kit_mt.lib
            libcrypto_mt.lib
            libssl_mt.lib
            zlib_mt.lib
            libucrt.lib
            libvcruntime.lib
            libcmt.lib
            LIBCPMT.LIB
        )
    else()
        target_link_libraries(rithmic_zorro_plugin PRIVATE
            RApiPlus_mt64.lib
            OmneStreamEngine_mt64.lib
            OmneChannel_mt64.lib
            OmneEngine_mt64.lib
            api_mt64.lib
            apistb_mt64.lib
            kit_mt64.lib
            libcrypto_mt64.lib
            libssl_mt64.lib
            zlib_mt64.lib
            libucrt.lib
            libvcruntime.lib
            libcmt.lib
            LIBCPMT.LIB
        )
    endif()
endif()

target_link_libraries(rithmic_zorro_plugin PRIVATE
    ws2_32.lib
    iphlpapi.lib
    psapi.lib
    legacy_stdio_definitions.lib
    OLDNAMES.lib
    bcrypt.lib
    crypt32.lib
    spdlog::spdlog_header_only
    user32.lib
    comctl32.lib # For dialog controls
)

if (CMAKE_BUILD_TYPE MATCHES Release)
    if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "win32")
        add_custom_command(TARGET rithmic_zorro_plugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
            COMMAND ${CMAKE_COMMAND} -E copy "${RITHMIC_INCLUDE_DIR}/../etc/rithmic_ssl_cert_auth_params" "${CMAKE_BINARY_DIR}/bin/Release/"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/rithmic.bin" "${CMAKE_BINARY_DIR}/bin/Release/"
            COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/RithmicZorroPlugin_${BUILD_VERSION}.zip" --format=zip "./"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Release"
            COMMENT "Creating zip archive"
        )
    else()
        add_custom_command(TARGET rithmic_zorro_plugin
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
            COMMAND ${CMAKE_COMMAND} -E copy "${RITHMIC_INCLUDE_DIR}/../etc/rithmic_ssl_cert_auth_params" "${CMAKE_BINARY_DIR}/bin/Release/"
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/rithmic.bin" "${CMAKE_BINARY_DIR}/bin/Release/"
            COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/RithmicZorroPlugin64_${BUILD_VERSION}.zip" --format=zip "./"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Release"
            COMMENT "Creating zip archive 64bit"
        )
    endif()

    if (DEFINED ENV{ZORRO_ROOT})
        if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "win32")
            add_custom_command(TARGET rithmic_zorro_plugin
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/Release/Rithmic.dll $ENV{ZORRO_ROOT}/plugin/Rithmic.dll
            )
        else()
            add_custom_command(TARGET rithmic_zorro_plugin
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/Release/Rithmic.dll $ENV{ZORRO_ROOT}/plugin64/Rithmic.dll
            )
        endif()
    endif()
endif()