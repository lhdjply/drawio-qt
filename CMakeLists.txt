cmake_minimum_required(VERSION 3.21)

project(drawio-qt
    VERSION 1.0.0
    DESCRIPTION "Native Qt6/C++ port of the draw.io desktop application"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport Xml)
find_package(ZLIB REQUIRED)

option(DRAWIO_QT_BUILD_TESTS "Build the unit tests" ON)
option(DRAWIO_QT_WARNINGS "Enable extra compiler warnings" ON)

set(DRAWIO_QT_SOURCES
    src/core/Style.cpp
    src/core/Style.h
    src/core/Cell.cpp
    src/core/Cell.h
    src/core/Page.cpp
    src/core/Page.h
    src/core/Document.cpp
    src/core/Document.h
    src/core/Compression.cpp
    src/core/Compression.h
    src/core/MxFileIO.cpp
    src/core/MxFileIO.h
    src/core/FileMeta.cpp
    src/core/FileMeta.h
    src/render/ShapeRegistry.cpp
    src/render/ShapeRegistry.h
    src/render/ShapeRenderer.cpp
    src/render/ShapeRenderer.h
    src/render/EdgeRouter.cpp
    src/render/EdgeRouter.h
    src/render/EdgeRenderer.cpp
    src/render/EdgeRenderer.h
    src/render/DiagramRenderer.cpp
    src/render/DiagramRenderer.h
    src/io/Exporter.cpp
    src/io/Exporter.h
    src/io/HtmlExport.cpp
    src/io/HtmlExport.h
    src/app/Args.cpp
    src/app/Args.h
    src/app/CliExport.cpp
    src/app/CliExport.h
    src/app/AppSettings.cpp
    src/app/AppSettings.h
    src/app/UiTheme.cpp
    src/app/UiTheme.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/DiagramView.cpp
    src/ui/DiagramView.h
    src/ui/ShapeToolbox.cpp
    src/ui/ShapeToolbox.h
    src/ui/FormatPanel.cpp
    src/ui/FormatPanel.h
    src/ui/OverlayEditor.cpp
    src/ui/OverlayEditor.h
    src/ui/Dialogs.cpp
    src/ui/Dialogs.h
)

add_library(drawioqt_lib STATIC ${DRAWIO_QT_SOURCES})

target_include_directories(drawioqt_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(drawioqt_lib PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::PrintSupport
    Qt6::Xml
    ZLIB::ZLIB)

target_compile_definitions(drawioqt_lib PUBLIC
    DRAWIO_QT_VERSION="${PROJECT_VERSION}")

if (DRAWIO_QT_WARNINGS)
    if (MSVC)
        target_compile_options(drawioqt_lib PRIVATE /W4)
    else()
        target_compile_options(drawioqt_lib PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

qt_add_resources(drawioqt_lib "drawioqt_resources"
    PREFIX "/"
    BASE "resources"
    FILES
        resources/icons/app.png
        resources/icons/new.svg
        resources/icons/open.svg
        resources/icons/save.svg
        resources/icons/undo.svg
        resources/icons/redo.svg
        resources/icons/delete.svg
        resources/icons/zoom-in.svg
        resources/icons/zoom-out.svg
        resources/icons/fit.svg
        resources/icons/print.svg
        resources/icons/export.svg
        resources/icons/grid.svg
        resources/icons/select.svg
        resources/icons/tab-close.svg
        resources/icons/tab-close-dark.svg
        resources/icons/connector.svg
        resources/icons/pan.svg
        resources/icons/text.svg
        resources/icons/image.svg
        resources/icons/group.svg
        resources/icons/ungroup.svg
        resources/icons/shapes/rectangle.svg
        resources/icons/shapes/rounded.svg
        resources/icons/shapes/ellipse.svg
        resources/icons/shapes/triangle.svg
        resources/icons/shapes/diamond.svg
        resources/icons/shapes/hexagon.svg
        resources/icons/shapes/cylinder.svg
        resources/icons/shapes/cloud.svg
        resources/icons/shapes/parallelogram.svg
        resources/icons/shapes/trapezoid.svg
        resources/icons/shapes/document.svg
        resources/icons/shapes/note.svg
        resources/icons/shapes/callout.svg
        resources/icons/shapes/actor.svg
        resources/icons/shapes/arrow.svg
        resources/icons/shapes/text.svg
        resources/icons/shapes/line.svg
        resources/icons/shapes/image.svg
        resources/icons/shapes/swimlane.svg
        resources/icons/shapes/process.svg
        resources/icons/shapes/circle.svg
        resources/icons/shapes/database.svg
        resources/icons/shapes/terminator.svg
        resources/icons/shapes/step.svg
        resources/examples/basic-flow.drawio
        resources/examples/uml-class.drawio
        resources/examples/network.drawio
)

add_executable(drawio-qt src/main.cpp)
target_link_libraries(drawio-qt PRIVATE drawioqt_lib)

set_target_properties(drawio-qt PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "drawio-qt"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.drawioqt"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

install(TARGETS drawio-qt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if (UNIX AND NOT APPLE)
    install(FILES resources/icons/app.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME drawio-qt.png)
    install(FILES packaging/io.github.drawioqt.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/drawio-qt-mime.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages
        RENAME drawio-qt.xml)
    install(DIRECTORY resources/examples/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/drawio-qt/examples)
endif()

install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/drawio-qt)

include(CMakePackageConfigHelpers)

if (DRAWIO_QT_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()

    set(DRAWIO_QT_TESTS
        tst_args
        tst_model
        tst_mxfile
        tst_filemeta
        tst_router
        tst_export
        tst_ui_smoke
    )

    foreach(test_name IN LISTS DRAWIO_QT_TESTS)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE drawioqt_lib Qt6::Test)
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            TIMEOUT 120)

        target_compile_definitions(${test_name} PRIVATE
            DRAWIO_QT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            DRAWIO_QT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    endforeach()
endif()
