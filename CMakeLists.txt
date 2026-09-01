cmake_minimum_required(VERSION 3.24)

project(
  rillnet
  VERSION 0.1.0
  DESCRIPTION "A stateful protocol framework"
  LANGUAGES CXX
)

option(RILLNET_BUILD_TESTS "Build rillnet tests" ON)
option(RILLNET_ENABLE_WARNINGS "Enable project compiler warnings" ON)
option(RILLNET_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)

find_package(Boost 1.83 REQUIRED)

function(rillnet_apply_project_warnings target_name)
  if(NOT RILLNET_ENABLE_WARNINGS)
    return()
  endif()

  set(rillnet_warning_flags)

  if(MSVC)
    list(APPEND rillnet_warning_flags /W4 /permissive-)
    if(RILLNET_WARNINGS_AS_ERRORS)
      list(APPEND rillnet_warning_flags /WX)
    endif()
  else()
    list(APPEND rillnet_warning_flags -Wall -Wextra -Wpedantic)
    if(RILLNET_WARNINGS_AS_ERRORS)
      list(APPEND rillnet_warning_flags -Werror)
    endif()
  endif()

  target_compile_options(${target_name} PRIVATE ${rillnet_warning_flags})
endfunction()

add_library(rillnet
  src/rillnet.cpp
)
add_library(rillnet::rillnet ALIAS rillnet)

target_compile_features(rillnet PUBLIC cxx_std_20)
set_target_properties(rillnet PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_include_directories(rillnet
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(rillnet PUBLIC Boost::headers)

rillnet_apply_project_warnings(rillnet)

include(CTest)

if(RILLNET_BUILD_TESTS AND BUILD_TESTING)
  find_package(GTest REQUIRED)
  add_subdirectory(tests)
endif()