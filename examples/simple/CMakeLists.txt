cmake_minimum_required(VERSION 3.20)
project(simple LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS Quick)
find_package(Qt6 COMPONENTS ShaderTools)

set(imgui_sources
    ../../imgui/imgui.cpp
    ../../imgui/imgui_draw.cpp
    ../../imgui/imgui_tables.cpp
    ../../imgui/imgui_widgets.cpp
    ../../imgui/imgui_demo.cpp
)

set(qrhiimgui_sources
    ../../qrhiimgui/qrhiimguiitem.cpp
    ../../qrhiimgui/qrhiimguiitem.h
    ../../qrhiimgui/qrhiimguinode.cpp
    ../../qrhiimgui/qrhiimguinode_p.h
)

qt_add_executable(simple
    main.cpp
    imguiitem.cpp imguiitem.h
    ${imgui_sources}
    ${qrhiimgui_sources}
)

target_include_directories(simple PUBLIC
    ../../imgui
    ../../qrhiimgui
)

target_link_libraries(simple PUBLIC
    Qt::Core
    Qt::GuiPrivate
    Qt::QuickPrivate
)

qt_add_qml_module(simple
    URI ImguiExample
    VERSION 1.0
    QML_FILES
        main.qml
    RESOURCE_PREFIX
        /
    NO_RESOURCE_TARGET_PATH
)

TARGET_COMPILE_DEFINITIONS(simple
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

qt6_add_shaders(simple "imgui_shaders"
    PREFIX
        "/"
    BASE
        "../../qrhiimgui"
    FILES
        "../../qrhiimgui/imgui.vert"
        "../../qrhiimgui/imgui.frag"
)
