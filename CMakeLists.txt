# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 HandsomeTurtle0307
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License.

cmake_minimum_required(VERSION 3.21)

project(MysticTurtle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)

qt_standard_project_setup()

set(MYSTIC_TURTLE_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h

    src/Theme.cpp
    src/Theme.h
    src/IconFactory.cpp
    src/IconFactory.h
    src/SampleData.cpp
    src/SampleData.h

    src/account/AccountManager.cpp
    src/account/AccountManager.h
    src/account/AccountDialog.cpp
    src/account/AccountDialog.h

    src/network/DownloadTask.cpp
    src/network/DownloadTask.h
    src/network/DownloadManager.cpp
    src/network/DownloadManager.h
    src/network/MojangMirror.cpp
    src/network/MojangMirror.h

    src/minecraft/VersionParser.cpp
    src/minecraft/VersionParser.h
    src/minecraft/GameInstaller.cpp
    src/minecraft/GameInstaller.h
    src/minecraft/GameLauncher.cpp
    src/minecraft/GameLauncher.h
    src/minecraft/JavaRuntimeInstaller.cpp
    src/minecraft/JavaRuntimeInstaller.h
    src/minecraft/LoaderDownloader.cpp
    src/minecraft/LoaderDownloader.h
    src/minecraft/VersionManifest.cpp
    src/minecraft/VersionManifest.h
    src/minecraft/MinecraftPaths.cpp
    src/minecraft/MinecraftPaths.h
    src/minecraft/LocalVersionScanner.cpp
    src/minecraft/LocalVersionScanner.h

    src/platform/WindowEffects.cpp
    src/platform/WindowEffects.h

    src/widgets/AccentButton.cpp
    src/widgets/AccentButton.h
    src/widgets/AnimatedComboBox.cpp
    src/widgets/AnimatedComboBox.h
    src/widgets/AnimatedStackedWidget.cpp
    src/widgets/AnimatedStackedWidget.h
    src/widgets/AvatarWidget.cpp
    src/widgets/AvatarWidget.h
    src/widgets/CardWidget.cpp
    src/widgets/CardWidget.h
    src/widgets/HeaderBar.cpp
    src/widgets/HeaderBar.h
    src/widgets/LogoMark.cpp
    src/widgets/LogoMark.h
    src/widgets/LoadingIndicator.cpp
    src/widgets/LoadingIndicator.h
    src/widgets/ModsDrawer.cpp
    src/widgets/ModsDrawer.h
    src/widgets/NavButton.cpp
    src/widgets/NavButton.h
    src/widgets/NavigationBar.cpp
    src/widgets/NavigationBar.h
    src/widgets/SearchField.cpp
    src/widgets/SearchField.h
    src/widgets/ToggleSwitch.cpp
    src/widgets/ToggleSwitch.h
    src/widgets/VersionCard.cpp
    src/widgets/VersionCard.h
    src/widgets/WindowButton.cpp
    src/widgets/WindowButton.h

    src/pages/HomePage.cpp
    src/pages/HomePage.h
    src/pages/LoaderSetupView.cpp
    src/pages/LoaderSetupView.h
    src/pages/DownloadGamePage.cpp
    src/pages/DownloadGamePage.h
    src/pages/DownloadModsPage.cpp
    src/pages/DownloadModsPage.h
    src/pages/SettingsPage.cpp
    src/pages/SettingsPage.h
    src/pages/AboutPage.cpp
    src/pages/AboutPage.h
)

qt_add_executable(Mystic-Turtle-Launcher
    WIN32
    MACOSX_BUNDLE
    ${MYSTIC_TURTLE_SOURCES}
)

target_include_directories(Mystic-Turtle-Launcher
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/minecraft
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pages
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
)

target_link_libraries(Mystic-Turtle-Launcher
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Concurrent
)

if(WIN32)
    target_compile_definitions(Mystic-Turtle-Launcher PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(Mystic-Turtle-Launcher PRIVATE dwmapi)
endif()

if(MSVC)
    target_compile_options(Mystic-Turtle-Launcher PRIVATE /utf-8 /W3)
else()
    target_compile_options(Mystic-Turtle-Launcher PRIVATE
        -Wall -Wextra -Wpedantic
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
endif()

set_target_properties(Mystic-Turtle-Launcher PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS Mystic-Turtle-Launcher
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Mystic-Turtle-Launcher
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
