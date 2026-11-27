cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
include(build.cmake)

project(quick-system-info-gui
	VERSION 3.0
	DESCRIPTION "Quick System Info - A utility to show quick-system-info (inxi, etc...) in a gui"
	LANGUAGES CXX
)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_standard_project_setup()

qt_add_executable(quick-system-info-gui
	main.cpp
	about.cpp about.h
	mainwindow.cpp mainwindow.h mainwindow.ui
	qsi_resource.qrc
)

target_link_libraries(quick-system-info-gui PRIVATE
	Qt::Core
	Qt::Gui
	Qt::Widgets
	archive
)

option(APPIMAGE_BUILD "Build the relocatable AppImage" OFF)
if(APPIMAGE_BUILD)
    target_compile_definitions(quick-system-info-gui PRIVATE APPIMAGE_BUILD)
endif()

build_compilation_setup(quick-system-info-gui)
build_version_definition(quick-system-info-gui VERSION)
build_translation_files(quick-system-info-gui translations)

include(GNUInstallDirs)
file(RELATIVE_PATH bindir_to_prefix "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_PREFIX}")
target_compile_definitions(quick-system-info-gui PRIVATE QSI_PREFIX_FROM_BIN="${bindir_to_prefix}")

install(TARGETS quick-system-info-gui
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(PROGRAMS lib/qsig-lib lib/qsig-lib-list DESTINATION lib/${PROJECT_NAME})
install(FILES Quick_System_Info.desktop DESTINATION share/applications)
install(FILES icons/mx-qsi.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES help/license.html help/quick-system-info-gui.html help/1.png help/2.png
    DESTINATION share/doc/${PROJECT_NAME})
install(FILES help/quick-system-info-gui.1 DESTINATION share/man/man1)

# Polkit matches the absolute executable path. Expand the final prefix during
# installation so that --prefix and staged package installations both work.
set(policy org.mxlinux.quick-system-info-gui.policy)
install(CODE "
    file(READ \"${CMAKE_CURRENT_SOURCE_DIR}/actions/${policy}\" policy_text)
    string(REPLACE \"/usr/lib/quick-system-info-gui/\" \"\${CMAKE_INSTALL_PREFIX}/lib/quick-system-info-gui/\"
        policy_text \"\${policy_text}\")
    file(WRITE \"${CMAKE_CURRENT_BINARY_DIR}/${policy}\" \"\${policy_text}\")
")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${policy}" DESTINATION share/polkit-1/actions)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS debian/changelog)
execute_process(COMMAND gzip -nc "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog"
    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz" COMMAND_ERROR_IS_FATAL ANY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/changelog.gz" DESTINATION share/doc/${PROJECT_NAME})
