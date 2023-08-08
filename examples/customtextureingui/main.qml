import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ImguiTextureExample

Rectangle {
    color: "lightGray"

    Imgui {
        id: gui
        objectName: "gui"
        anchors.fill: parent
    }
}
