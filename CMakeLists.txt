# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22)
project(hover_translate VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent StateMachine)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED IMPORTED_TARGET tesseract)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)

add_library(hover_core STATIC
    third_party/crow/onlinetranslator.cpp
    third_party/crow/onlinetranslator.h
    third_party/crow/aocrprovider.h
    third_party/crow/tesseractocr.cpp
    third_party/crow/tesseractocr.h
    src/settings.cpp src/settings.h
    src/hoverpolicy.cpp src/hoverpolicy.h
    src/translationservice.cpp src/translationservice.h
    src/x11escape.cpp src/x11escape.h
    src/translationpopup.cpp src/translationpopup.h
    src/hovercontroller.cpp src/hovercontroller.h
)
target_include_directories(hover_core PUBLIC src third_party/crow)
target_link_libraries(hover_core PUBLIC Qt6::Widgets Qt6::Network Qt6::Concurrent
    Qt6::StateMachine PkgConfig::TESSERACT PkgConfig::XCB)
target_compile_options(hover_core PRIVATE -Wall -Wextra -Wpedantic)
add_executable(hover-translate src/main.cpp src/settingsdialog.cpp src/settingsdialog.h resources.qrc)
target_link_libraries(hover-translate PRIVATE hover_core)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
install(TARGETS hover-translate RUNTIME DESTINATION bin)
install(FILES packaging/io.github.LincolnGothic.HoverTranslate.desktop DESTINATION share/applications)
install(FILES packaging/hover-translate.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES LICENSE NOTICE.md DESTINATION share/doc/hover-translate)
install(DIRECTORY LICENSES/ DESTINATION share/doc/hover-translate/licenses)
install(FILES README.md README.zh-CN.md CONTRIBUTING.md CHANGELOG.md
    DESTINATION share/doc/hover-translate)
install(DIRECTORY docs/ DESTINATION share/doc/hover-translate/docs)
set(CPACK_PACKAGE_NAME hover-translate)
set(CPACK_PACKAGE_VERSION 0.1.0)
set(CPACK_PACKAGE_CONTACT "LincolnGothic")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.38), libstdc++6 (>= 14), libgcc-s1, libqt6core6t64 (>= 6.8), libqt6gui6 (>= 6.8), libqt6widgets6 (>= 6.8), libqt6network6 (>= 6.8), libqt6concurrent6 (>= 6.8), libqt6statemachine6 (>= 6.8), qt6-qpa-plugins, qt6-svg-plugins, libtesseract5 (>= 5.0), libleptonica6, libxcb1, tesseract-ocr-eng, tesseract-ocr-chi-sim, fonts-noto-cjk")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "X11 mouse-hover translation between English and Simplified Chinese")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/LincolnGothic/Linux_mouse_hover_translation")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "hover-translate-${PROJECT_VERSION}-Source")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/release")
set(CPACK_SOURCE_IGNORE_FILES "/build[^/]*/;/[.]git/;/[.]tools/;/_CPack_Packages/;__pycache__;[.]user$;[.]deb$;[.]tar[.]gz$")
include(CPack)
