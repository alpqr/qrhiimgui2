cmake_minimum_required(VERSION 3.20)
project(simple LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS Quick)
find_package(Qt6 COMPONENTS ShaderTools)

set(imgui_target simple)
set(imgui_base ../../imgui)
include(${imgui_base}/imguipre.cmakeinc)

qt_add_executable(simple
    main.cpp
    imguiitem.h
    ${imgui_sources}
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

include(${imgui_base}/imguipost.cmakeinc)
