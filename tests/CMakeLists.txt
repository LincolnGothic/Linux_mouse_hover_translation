# SPDX-FileCopyrightText: 2026 Linux_mouse_hover_translation contributors
# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(test_core test_core.cpp mockserver.h)
target_link_libraries(test_core PRIVATE hover_core Qt6::Test)
add_test(NAME core COMMAND test_core)

pkg_check_modules(XCB_XTEST REQUIRED IMPORTED_TARGET xcb-xtest)
add_executable(test_hover test_hover.cpp mockserver.h)
target_link_libraries(test_hover PRIVATE hover_core Qt6::Test PkgConfig::XCB_XTEST)
target_compile_definitions(test_hover PRIVATE APP_BINARY="$<TARGET_FILE:hover-translate>")
add_dependencies(test_hover hover-translate)
add_test(NAME hover_x11 COMMAND test_hover)
add_executable(test_portal test_portal.cpp)
target_link_libraries(test_portal PRIVATE hover_core Qt6::Test)
add_test(NAME portal_capture COMMAND test_portal)
set_tests_properties(core hover_x11 portal_capture PROPERTIES TIMEOUT 60)
