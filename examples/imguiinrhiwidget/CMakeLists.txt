cmake_minimum_required(VERSION 3.20)
project(imguiinrhiwidget LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets ShaderTools)

qt_add_executable(imguiinrhiwidget
    main.cpp
)

set(imgui_base ../../imgui)
set(imgui_target imguiinrhiwidget)
include(${imgui_base}/imgui.cmakeinc)

target_link_libraries(imguiinrhiwidget PUBLIC
    Qt::Core
    Qt::GuiPrivate
    Qt::Widgets
)

qt6_add_shaders(imguiinrhiwidget "shaders"
    PREFIX
        "/shaders"
    FILES
        color.vert
        color.frag
)

qt6_add_resources(imguiinrhiwidget "resources"
    PREFIX
        "/"
    BASE
        "../shared"
    FILES
        "../shared/fonts/RobotoMono-Medium.ttf"
)
