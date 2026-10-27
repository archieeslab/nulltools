cmake_minimum_required(VERSION 3.20)

project(nullroute VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(WIN32)
  enable_language(RC)
  file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico" NR_ICON)
  set(NR_VERSION "${PROJECT_VERSION}")
  set(NR_VERSION_COMMA
      "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
endif()

add_library(nr STATIC src/nr.cpp src/hash.cpp src/ops.cpp src/web.cpp src/deps.cpp)
target_include_directories(nr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(nr PRIVATE /W4 /permissive-)
else()
  target_compile_options(nr PRIVATE -Wall -Wextra)
endif()

if(WIN32)
  target_link_libraries(nr PUBLIC ws2_32 shell32 user32)
endif()

set(NR_ASSET_SOURCES)
foreach(pair "index.html:index_html" "logo.png:logo_png")
  string(REPLACE ":" ";" parts "${pair}")
  list(GET parts 0 asset_file)
  list(GET parts 1 asset_name)
  set(NR_ASSET_IN "${CMAKE_CURRENT_SOURCE_DIR}/resources/web/${asset_file}")
  set(NR_ASSET_NAME "${asset_name}")
  set(NR_ASSET_OUT "${CMAKE_CURRENT_BINARY_DIR}/asset_${asset_name}.cpp")
  execute_process(
    COMMAND ${CMAKE_COMMAND}
      "-DNR_ASSET_IN=${NR_ASSET_IN}"
      "-DNR_ASSET_NAME=${NR_ASSET_NAME}"
      "-DNR_ASSET_OUT=${NR_ASSET_OUT}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake"
    RESULT_VARIABLE NR_ASSET_STATUS)
  if(NOT NR_ASSET_STATUS EQUAL 0)
    message(FATAL_ERROR "failed to embed ${asset_file}")
  endif()
  list(APPEND NR_ASSET_SOURCES "${NR_ASSET_OUT}")
endforeach()

target_sources(nr PRIVATE ${NR_ASSET_SOURCES})

set(NR_TOOLS
  nrinit
  nrscan
  nrtrace
  nrcrack
  nrdump
  nrwatch
  nrpe
  nrfind
  nrstrings
  nrhash
  nrhex)

function(nr_add_tool target output description)
  add_executable(${target} tools/${target}.cpp)
  target_link_libraries(${target} PRIVATE nr)

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    target_link_options(${target} PRIVATE -static)
  endif()

  if(WIN32)
    set(NR_INTERNAL "${output}")
    set(NR_DESCRIPTION "${description}")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/${target}.rc" @ONLY)
    add_library(${target}_res OBJECT "${CMAKE_CURRENT_BINARY_DIR}/${target}.rc")
    target_link_libraries(${target} PRIVATE ${target}_res)
  endif()

  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${output}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin")
endfunction()

foreach(tool IN LISTS NR_TOOLS)
  nr_add_tool(${tool} ${tool} "nullroute ${tool}")
endforeach()

nr_add_tool(nullroute nullroute "nullroute console")
nr_add_tool(nullroute_legacy nullroute-legacy "nullroute legacy terminal")
