cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------
# 版本号只有一个来源：仓库根目录的 VERSION 文件
#
#   file(STRINGS ... LIMIT_COUNT 1) 只取第一行；下面的 string(STRIP) 顺带吃掉
#   行尾的 \r —— Windows 上若按 CRLF 检出，版本号会带进一个回车，
#   project(VERSION ...) 会直接解析失败。
# ---------------------------------------------------------------------------
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" DRAWIO_QT_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${DRAWIO_QT_VERSION_STRING}" DRAWIO_QT_VERSION_STRING)
if(DRAWIO_QT_VERSION_STRING STREQUAL "")
    message(FATAL_ERROR
            "无法从 ${CMAKE_CURRENT_SOURCE_DIR}/VERSION 读取版本号（文件缺失或为空）")
endif()

# 文件里按 tag 的写法带 v 前缀（v1.0.0），但 CMake 的 project(VERSION) 只接受
# 纯数字；这里剥掉前缀给 CMake 用，原样保留一份给 `--version` 等展示用。
string(REGEX REPLACE "^[vV]" "" DRAWIO_QT_VERSION_NUMBER "${DRAWIO_QT_VERSION_STRING}")
if(NOT DRAWIO_QT_VERSION_NUMBER MATCHES "^[0-9]+(\\.[0-9]+)*$")
    message(FATAL_ERROR
            "VERSION 里的版本号不合法：'${DRAWIO_QT_VERSION_STRING}'（期望形如 v1.0.0 或 1.0.0）")
endif()

# file() 只在配置期读一次，CMake 不会自动把它登记成"重配置依赖"：少了这一行，
# 改完 VERSION 直接构建会继续用旧版本号（必须手工再 configure 一次）。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

project(drawio-qt
    VERSION ${DRAWIO_QT_VERSION_NUMBER}
    DESCRIPTION "Native Qt6/C++ port of the draw.io desktop application"
    LANGUAGES CXX)

# Debian 打包的版本号只能来自 debian/changelog（dpkg 的硬约束，不能动态生成）。
# 两份版本号不一致时给个提示：通常意味着改了 VERSION 却忘了 dch。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DRAWIO_QT_DEB_LINE
         LIMIT_COUNT 1)
    if(DRAWIO_QT_DEB_LINE MATCHES "^[^(]+\\(([^)]+)\\)")
        set(DRAWIO_QT_DEB_VERSION "${CMAKE_MATCH_1}")
        # 允许 Debian 修订号（0.1.0-1）：只比较上游版本部分
        string(REGEX REPLACE "-[0-9]+$" "" DRAWIO_QT_DEB_UPSTREAM "${DRAWIO_QT_DEB_VERSION}")
        if(NOT DRAWIO_QT_DEB_UPSTREAM STREQUAL DRAWIO_QT_VERSION_NUMBER)
            message(WARNING
                    "VERSION（${DRAWIO_QT_VERSION_STRING}）与 debian/changelog"
                    "（${DRAWIO_QT_DEB_VERSION}）不一致：安装包版本取 changelog，"
                    "发布前请用 `dch -v ${DRAWIO_QT_VERSION_NUMBER}` 同步")
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
#  应用标识
#
#  源码与打包文件里不再重复出现 "drawio-qt" 字面量：desktop 模板里的
#  Exec / Icon / StartupWMClass 与显示名都由这里的两个变量派生，
#  改软件名只需要改 project() 与显示名。
# ---------------------------------------------------------------------------
set(DRAWIO_QT_APP_NAME           "${PROJECT_NAME}")   # 可执行文件 / desktop ID
set(DRAWIO_QT_APP_DISPLAY_NAME   "drawio-qt")

include(GNUInstallDirs)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg PrintSupport Xml)
find_package(ZLIB REQUIRED)

option(DRAWIO_QT_BUILD_TESTS "Build the unit tests" OFF)
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
    src/render/ShapeCatalog.cpp
    src/render/ShapeCatalog.h
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
    src/ui/ShapeLibraryDialog.cpp
    src/ui/ShapeLibraryDialog.h
    src/ui/FormatPanel.cpp
    src/ui/FormatPanel.h
    src/ui/OverlayEditor.cpp
    src/ui/OverlayEditor.h
    src/ui/Dialogs.cpp
    src/ui/Dialogs.h
)

add_library(drawioqt_lib OBJECT ${DRAWIO_QT_SOURCES})

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

file(GLOB_RECURSE DRAWIO_IMAGE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/drawio/img/lib/*")

qt_add_resources(drawioqt_lib "drawio_image_resources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES ${DRAWIO_IMAGE_FILES})

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

# Windows：把图标写进 PE 资源段（qrc 只作用于运行期窗口图标）
if(WIN32)
    target_sources(drawio-qt PRIVATE resources/win/drawio-qt.rc)
endif()

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
    # 桌面入口是模板：Exec / Icon / StartupWMClass 与显示名都来自上面的
    # 变量，避免改名时漏掉其中一处。
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/drawio-qt.desktop.in"
        "${CMAKE_BINARY_DIR}/${DRAWIO_QT_APP_NAME}.desktop" @ONLY)
    install(FILES "${CMAKE_BINARY_DIR}/${DRAWIO_QT_APP_NAME}.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/app.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME drawio-qt.png)
    install(FILES resources/linux/drawio-qt-mime.xml
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
