cmake_minimum_required(VERSION 3.20)
project(mpvjoc LANGUAGES CXX)

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
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/audit_shortcut_help.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Auditing shortcut help coverage"
    VERBATIM
)

# Development helper: strict shortcut help audit. This target fails when possible
# missing F12 help entries are found.
add_custom_target(audit-shortcut-help-strict
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/audit_shortcut_help.py --strict
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Auditing shortcut help coverage in strict mode"
    VERBATIM
)

