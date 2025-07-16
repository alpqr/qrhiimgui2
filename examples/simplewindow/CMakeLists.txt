cmake_minimum_required(VERSION 3.20)
project(simplewindow LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 COMPONENTS Core Gui ShaderTools OPTIONAL_COMPONENTS GuiPrivate)

qt_add_executable(simplewindow
    main.cpp
)

set(imgui_base ../../imgui)
set(imgui_target simplewindow)
include(${imgui_base}/imgui.cmakeinc)

target_link_libraries(simplewindow PRIVATE
    Qt::Core
    Qt::GuiPrivate
)

qt6_add_shaders(simplewindow "shaders"
    PREFIX
        "/shaders"
    FILES
        color.vert
        color.frag
)

qt6_add_resources(simplewindow "resources"
    PREFIX
        "/"
    BASE
        "../shared"
    FILES
        "../shared/fonts/RobotoMono-Medium.ttf"
)
