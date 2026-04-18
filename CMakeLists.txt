cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

# ---------------------------------------------------------------------------
# Project metadata
# ---------------------------------------------------------------------------
project(${_name} VERSION ${_version} LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_HTTP_SERVER  "Serve overlay on http://127.0.0.1:<port>"  ON)
option(ENABLE_QT           "Use Qt Widgets/Core"                       ON)

include(compilerconfig)
include(defaults)
include(helpers)

if(APPLE)
  add_compile_options(
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=newline-eof>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=deprecated-this-capture>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=deprecated-declarations>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wno-error=range-loop-construct>
  )
  # Fix for AGL framework being missing in modern macOS SDKs
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
  set(AGL_LIBRARY "-framework OpenGL" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(LWS_SRC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LWS_THIRD_DIR  "${LWS_SRC_DIR}/thirdparty")
set(LWS_GEN_DIR    "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LWS_GEN_DIR}")

# Generate config.hpp from config.hpp.in into build dir
configure_file(
  "${LWS_SRC_DIR}/config.hpp.in"
  "${LWS_GEN_DIR}/config.hpp"
  @ONLY
)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(APPLE)
    # Patch OBS targets to remove legacy AGL framework references
    foreach(target OBS::libobs OBS::obs-frontend-api)
        if(TARGET ${target})
            get_target_property(libs ${target} INTERFACE_LINK_LIBRARIES)
            if(libs)
                list(REMOVE_ITEM libs "-framework AGL" "AGL")
                set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "${libs}")
            endif()
        endif()
    endforeach()

    # Link required macOS frameworks
    find_library(CARBON_FRAMEWORK Carbon REQUIRED)
    find_library(APP_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE 
        ${CARBON_FRAMEWORK} 
        ${APP_SERVICES_FRAMEWORK}
        ${COCOA_FRAMEWORK}
    )

    # Aggressively remove AGL from the target's link list
    get_target_property(TARGET_LIBS ${CMAKE_PROJECT_NAME} LINK_LIBRARIES)
    if(TARGET_LIBS)
        list(REMOVE_ITEM TARGET_LIBS "-framework AGL" "AGL")
        set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_LIBRARIES "${TARGET_LIBS}")
    endif()
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  "${LWS_SRC_DIR}"
  "${LWS_GEN_DIR}"
  "${LWS_THIRD_DIR}"
)

# ---------------------------------------------------------------------------
# Frontend API (Tools menu + current scene access)
# ---------------------------------------------------------------------------
if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_FRONTEND_API=1)
endif()

# ---------------------------------------------------------------------------
# Qt (Dialog UI)
# ---------------------------------------------------------------------------
if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_QT=1)

  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

# ---------------------------------------------------------------------------
# HTTP server deps
# ---------------------------------------------------------------------------
if(ENABLE_HTTP_SERVER)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ENABLE_HTTP_SERVER=1)
  if(WIN32)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ws2_32)
  endif()
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(LOCAL_WEBSERVER_SRC
  "${LWS_SRC_DIR}/plugin.cpp"
  "${LWS_SRC_DIR}/lws_dialog.cpp"
  "${LWS_SRC_DIR}/lws_paths.cpp"
  "${LWS_SRC_DIR}/lws_settings.cpp"
  "${LWS_SRC_DIR}/lws_server.cpp"
  "${LWS_SRC_DIR}/lws_obs_helpers.cpp"
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${LOCAL_WEBSERVER_SRC})

# ---------------------------------------------------------------------------
# Output name / plugin helper
# ---------------------------------------------------------------------------
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
