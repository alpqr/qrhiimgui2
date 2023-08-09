cmake_minimum_required(VERSION 3.20)
project(simplewindow LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS ShaderTools)

qt_add_executable(simplewindow
    main.cpp
)

set(imgui_base ../../imgui)
set(imgui_target simplewindow)
include(${imgui_base}/imgui.cmakeinc)

target_link_libraries(simplewindow PUBLIC
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
