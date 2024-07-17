cmake_minimum_required(VERSION 3.12)
project(hyprpaper
    DESCRIPTION "A blazing fast wayland wallpaper utility"
    VERSION 0.7.0
)

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Configuring hyprpaper!")

# Get git info
# hash and branch
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
    COMMAND bash -c "git show ${GIT_COMMIT_HASH} | head -n 5 | tail -n 1"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_MESSAGE
    OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND bash -c "git diff-index --quiet HEAD -- || echo \"dirty\""
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE)
#
#

include_directories(.)
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-DWLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET wayland-client wayland-protocols cairo pango pangocairo libjpeg libwebp hyprlang>=0.2.0 hyprutils>=0.2.0 hyprwayland-scanner>=0.4.0)

file(GLOB_RECURSE SRCFILES "src/*.cpp")

add_executable(hyprpaper ${SRCFILES})

pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")
pkg_get_variable(WAYLAND_CLIENT_DIR wayland-client pkgdatadir)
message(STATUS "Found wayland-client at ${WAYLAND_CLIENT_DIR}")

function(protocolNew protoPath protoName external)
    if (external)
        set(path ${CMAKE_SOURCE_DIR}/${protoPath})
    else()
        set(path ${WAYLAND_PROTOCOLS_DIR}/${protoPath})
    endif()
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/protocols/${protoName}.cpp
               ${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp
        COMMAND hyprwayland-scanner --client ${path}/${protoName}.xml ${CMAKE_SOURCE_DIR}/protocols/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    target_sources(hyprpaper PRIVATE protocols/${protoName}.cpp protocols/${protoName}.hpp)
endfunction()
function(protocolWayland)
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/protocols/wayland.cpp
               ${CMAKE_SOURCE_DIR}/protocols/wayland.hpp
        COMMAND hyprwayland-scanner --wayland-enums --client ${WAYLAND_CLIENT_DIR}/wayland.xml ${CMAKE_SOURCE_DIR}/protocols/
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    target_sources(hyprpaper PRIVATE protocols/wayland.cpp protocols/wayland.hpp)
endfunction()

protocolWayland()

protocolNew("protocols" "wlr-layer-shell-unstable-v1" true)
protocolNew("stable/linux-dmabuf" "linux-dmabuf-v1" false)
protocolNew("staging/fractional-scale" "fractional-scale-v1" false)
protocolNew("stable/viewporter" "viewporter" false)
protocolNew("stable/xdg-shell" "xdg-shell" false)
protocolNew("staging/cursor-shape" "cursor-shape-v1" false)
protocolNew("stable/tablet" "tablet-v2" false)

target_compile_definitions(hyprpaper PRIVATE "-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
target_compile_definitions(hyprpaper PRIVATE "-DGIT_BRANCH=\"${GIT_BRANCH}\"")
target_compile_definitions(hyprpaper PRIVATE "-DGIT_COMMIT_MESSAGE=\"${GIT_COMMIT_MESSAGE}\"")
target_compile_definitions(hyprpaper PRIVATE "-DGIT_DIRTY=\"${GIT_DIRTY}\"")

target_link_libraries(hyprpaper rt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

target_link_libraries(hyprpaper PkgConfig::deps)

target_link_libraries(hyprpaper
        OpenGL
        GLESv2
        pthread
        magic
        ${CMAKE_THREAD_LIBS_INIT}
        wayland-cursor
)

IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -no-pie -fno-builtin")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg -no-pie -fno-builtin")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg -no-pie -fno-builtin")
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)

install(TARGETS hyprpaper)
