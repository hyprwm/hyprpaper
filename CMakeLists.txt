cmake_minimum_required(VERSION 3.12)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VERSION)

project(
  hyprpaper
  DESCRIPTION "A blazing fast wayland wallpaper utility"
  VERSION ${VERSION})

add_compile_definitions(HYPRPAPER_VERSION="${VERSION}")

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Configuring hyprpaper!")

configure_file(systemd/hyprpaper.service.in systemd/hyprpaper.service @ONLY)

# Get git info hash and branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
  COMMAND bash -c "git show ${GIT_COMMIT_HASH} | head -n 5 | tail -n 1 | sed -s 's/\#//g; s/^[[:space:]]*//'"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_MESSAGE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
  COMMAND bash -c "git diff-index --quiet HEAD -- || echo \"dirty\""
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_DIRTY
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories(. hw-protocols protocols)
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-DWLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value
                    -Wno-missing-field-initializers -Wno-narrowing)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(hyprwayland-scanner 0.4.0 REQUIRED)

pkg_check_modules(
  deps
  REQUIRED
  IMPORTED_TARGET
  hyprlang>=0.6.0
  hyprutils>=0.2.4
  hyprtoolkit>=0.4.1
  hyprwire
  pixman-1
  libdrm)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

add_executable(hyprpaper ${SRCFILES})

# Hyprwire

pkg_get_variable(HYPRWIRE_PROTOCOLS_DIR hyprwire-protocols pkgdatadir)
message(STATUS "Found hyprwire-protocols at ${HYPRWIRE_PROTOCOLS_DIR}")

function(hyprprotocolServer protoPath protoName)
  set(path ${CMAKE_SOURCE_DIR}/${protoPath})
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/hw-protocols/${protoName}-server.cpp
           ${CMAKE_SOURCE_DIR}/hw-protocols/${protoName}-server.hpp
    COMMAND hyprwire-scanner ${path}/${protoName}.xml
            ${CMAKE_SOURCE_DIR}/hw-protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(hyprpaper PRIVATE hw-protocols/${protoName}-server.cpp
                                   hw-protocols/${protoName}-server.hpp
                                   )
endfunction()

function(hyprprotocol protoPath protoName external)
  if(external)
    set(path ${protoPath})
  else()
    set(path ${HYPRWIRE_PROTOCOLS_DIR}/${protoPath})
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.hpp
    COMMAND hyprwire-scanner --client ${path}/${protoName}.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(hyprpaper PRIVATE hw-protocols/${protoName}-client.cpp
                                   hw-protocols/${protoName}-client.hpp
                                   )
endfunction()

hyprprotocolServer(hw-protocols hyprpaper_core)

hyprprotocol(hyprtavern hp_hyprtavern_core_v1 false)

# 

string(REPLACE "\"" " " GIT_COMMIT_MESSAGE_ESCAPED "${GIT_COMMIT_MESSAGE}")
target_compile_definitions(hyprpaper
                           PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
target_compile_definitions(hyprpaper PRIVATE "-DGIT_BRANCH=\"${GIT_BRANCH}\"")
target_compile_definitions(hyprpaper
                           PRIVATE "-DGIT_COMMIT_MESSAGE=\"${GIT_COMMIT_MESSAGE_ESCAPED}\"")
target_compile_definitions(hyprpaper PRIVATE "-DGIT_DIRTY=\"${GIT_DIRTY}\"")

target_link_libraries(hyprpaper rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_link_libraries(hyprpaper PkgConfig::deps)

target_link_libraries(
  hyprpaper
  OpenGL
  GLESv2
  pthread
  magic
  ${CMAKE_THREAD_LIBS_INIT}
  wayland-cursor)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -no-pie -fno-builtin")
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -pg -no-pie -fno-builtin")
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -pg -no-pie -fno-builtin")
endif(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)

include(GNUInstallDirs)

install(TARGETS hyprpaper)
install(FILES ${CMAKE_BINARY_DIR}/systemd/hyprpaper.service DESTINATION "lib/systemd/user")
