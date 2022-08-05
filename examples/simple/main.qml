import QtQuick
import QtQuick.Controls
import ImguiExample

Item {
    Rectangle {
        color: "red"
        anchors.fill: parent
        Rectangle {
            color: "lightGray"
            // for testing only, this is not intended to be 100% usable in practice
//            width: 1000
//            height: 600
//            anchors.centerIn: parent
//            border.color: "green"
//            border.width: 4

            // this is the intended usage, covering the whole screen (and so input matches 1:1)
            anchors.fill: parent

            Imgui {
                id: gui
                anchors.fill: parent
            }
        }
        Button {
            y: 20
            text: "Toggle"
            onClicked: gui.visible = !gui.visible
        }
        TextEdit {
            id: textEdit
            x: 100
            y: 20
            width: 500
            text: "focus test"
            font.pointSize: 20
            color: "blue"
        }
    }
}
