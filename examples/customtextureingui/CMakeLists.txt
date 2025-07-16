cmake_minimum_required(VERSION 3.20)
project(customtextureingui LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 COMPONENTS Core Gui Quick ShaderTools OPTIONAL_COMPONENTS GuiPrivate QuickPrivate)

qt_add_executable(customtextureingui
    main.cpp
    imguiitem.h
    triangle.cpp triangle.h
)

set(imgui_base ../../imgui)
set(imgui_target customtextureingui)
include(${imgui_base}/imgui.cmakeinc)
include(${imgui_base}/imguiquick.cmakeinc)

target_link_libraries(customtextureingui PRIVATE
    Qt::Core
    Qt::GuiPrivate
    Qt::QuickPrivate
)

qt_add_qml_module(customtextureingui
    URI ImguiTextureExample
    VERSION 1.0
    QML_FILES
        main.qml
    RESOURCE_PREFIX
        /
    NO_RESOURCE_TARGET_PATH
)

TARGET_COMPILE_DEFINITIONS(customtextureingui
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

qt_add_resources(customtextureingui "resources"
    PREFIX
        "/"
    BASE
        "../shared"
    FILES
        "../shared/fonts/RobotoMono-Medium.ttf"
)

qt_add_shaders(customtextureingui "shaders"
    PREFIX
        "/shaders"
    FILES
        color.vert
        color.frag
)
