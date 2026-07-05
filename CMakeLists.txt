cmake_minimum_required(VERSION 3.20)
project(mpvjoc LANGUAGES CXX)


set(MPVJOC_VERSION "0.9.0" CACHE STRING "mpvjoc semantic version")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE MPVJOC_GIT_COMMIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE MPVJOC_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE MPVJOC_GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" diff --quiet
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE MPVJOC_GIT_DIRTY_RESULT
        ERROR_QUIET)
    if(MPVJOC_GIT_DIRTY_RESULT EQUAL 0)
        set(MPVJOC_GIT_DIRTY "clean")
    else()
        set(MPVJOC_GIT_DIRTY "dirty")
    endif()
else()
    set(MPVJOC_GIT_COMMIT_COUNT "0")
    set(MPVJOC_GIT_HASH "unknown")
    set(MPVJOC_GIT_DESCRIBE "unknown")
    set(MPVJOC_GIT_DIRTY "unknown")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
    @ONLY)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

add_executable(mpvjoc
    main.cpp
    util/Utils.cpp
    util/TextHighlighting.cpp
    mpv/MpvWidget.cpp
    mpv/PlaybackOverlayWidget.cpp
    playlist/PlaylistModel.cpp
    playlist/PlaylistFilterProxyModel.cpp
    playlist/PlaylistDelegate.cpp
    playlist/PlaylistFormatting.cpp
    playlist/PlaylistBadges.cpp
    playlist/PlaylistDurationRuns.cpp
    media/MetadataProbeManager.cpp
    media/ThumbnailManager.cpp
    ui/TimelineWidget.cpp
    ui/MainWindow.cpp
    ui/MainWindowShortcuts.cpp
    ui/MainWindowDrops.cpp
    ui/MainWindowState.cpp
    ui/MainWindowPlaylist.cpp
    ui/MainWindowPlayback.cpp
    ui/MainWindowMove.cpp
    ui/MainWindowMedia.cpp
    ui/MainWindowFileActions.cpp
    ui/MainWindowUi.cpp
)

target_include_directories(mpvjoc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(mpvjoc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/mpv
    ${CMAKE_CURRENT_SOURCE_DIR}/playlist
    ${CMAKE_CURRENT_SOURCE_DIR}/media
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

target_link_libraries(mpvjoc PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    PkgConfig::MPV
)

# Development helper: audit shortcuts documented in the F12 help overlay.
# This target is explicit-only; it is not part of the normal build.
add_custom_target(audit-shortcut-help
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/audit_shortcut_help.py --context 140
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Auditing shortcut help coverage"
    VERBATIM
)

# Development helper: strict shortcut help audit. This target fails when possible
# missing F12 help entries are found.
add_custom_target(audit-shortcut-help-strict
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/audit_shortcut_help.py --strict --context 140
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Auditing shortcut help coverage in strict mode"
    VERBATIM
)

