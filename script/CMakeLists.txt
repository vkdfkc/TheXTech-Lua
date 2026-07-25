cmake_minimum_required (VERSION 3.5)
project(PGEFileLibrary C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
    message("== Using default build configuration which is a Release!")
endif()

if(POLICY CMP0077) # Allow external configuring when building as sub-directory
    cmake_policy(SET CMP0077 NEW)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_DEBUG_POSTFIX "d")

include(../cmake/build_props.cmake)

pge_cxx_standard(11)

set(LIB_XTECHLUA_SRCS
    src/xtech_lua_main.cpp
    src/xtech_lua_bindings.cpp
    src/xtech_lua_events.cpp
    src/xtech_lua_data.cpp
)

add_library(XTechLua STATIC
    ${LIB_XTECHLUA_SRCS}
)

target_include_directories(XTechLua PRIVATE
    include src lib
    ${TheXTech_SOURCE_DIR}/src
    ${TheXTech_SOURCE_DIR}/lib
    ${TheXTech_SOURCE_DIR}/3rdparty
    "${DEPENDENCIES_INSTALL_DIR}/include"
    "${DEPENDENCIES_INSTALL_DIR}/include/luajit"
    ${A2XT_INCS}
)

target_link_libraries(XTechLua PUBLIC PGE_LuaJIT PGE_LuaBind)
add_dependencies(XTechLua LuaBind_Local ${A2XT_DEPS})

# GCC 12 + RangeArr returns const refs from extern arrays
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0")
    target_compile_options(XTechLua PRIVATE -fpermissive)
endif()

# Inherit THEXTECH_FIXED_POINT from the main build (num_t layout must match)
if(THEXTECH_FIXED_POINT)
    target_compile_definitions(XTechLua PRIVATE -DTHEXTECH_FIXED_POINT)
endif()

# Inherit THEXTECH_BUILD_GL_MODERN from the main build (StdPictureData layout must match)
if(THEXTECH_BUILD_GL_DESKTOP_MODERN OR THEXTECH_BUILD_GL_ES_MODERN)
    target_compile_definitions(XTechLua PRIVATE -DTHEXTECH_BUILD_GL_MODERN)
endif()
