cmake_minimum_required(VERSION 3.20)
project(simplewindow LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS Quick)
find_package(Qt6 COMPONENTS ShaderTools)

set(imgui_base ../../imgui)
set(imgui_sources
    ${imgui_base}/imgui/imgui.cpp
    ${imgui_base}/imgui/imgui_draw.cpp
    ${imgui_base}/imgui/imgui_tables.cpp
    ${imgui_base}/imgui/imgui_widgets.cpp
    ${imgui_base}/imgui/imgui_demo.cpp
    ${imgui_base}/qrhiimgui.cpp
    ${imgui_base}/qrhiimgui_p.h
)

qt_add_executable(simplewindow
    main.cpp
    ${imgui_sources}
)

target_link_libraries(simplewindow PUBLIC
    Qt::Core
    Qt::GuiPrivate
)

target_include_directories(simplewindow PRIVATE
    ${imgui_base}
    ${imgui_base}/imgui
)

qt6_add_shaders(simplewindow "imgui_shaders"
    PREFIX
        "/"
    BASE
        ${imgui_base}
    FILES
        ${imgui_base}/imgui.vert
        ${imgui_base}/imgui.frag
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
    FILES
        fonts/RobotoMono-Medium.ttf
)
